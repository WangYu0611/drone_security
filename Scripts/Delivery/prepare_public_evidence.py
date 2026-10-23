"""Prepare only Git-untracked Evidence for public delivery.

Default is a read-only audit. After producers stop and images are reviewed:
  python -B Scripts/Delivery/prepare_public_evidence.py --date 20261023 --apply --images-reviewed
  python -B Scripts/Delivery/prepare_public_evidence.py --date 20261023 --verify-manifest

Tracked historical evidence is never rewritten. Changed originals are backed up
under ignored Saved/Delivery/raw-evidence/<UTC timestamp>/<repository path>.
The manifest records every selected file, including unchanged images and XML. Screenshot suffixes are preserved; their
actual PNG/JPEG formats are recorded separately.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys
from urllib.parse import unquote
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
BASE = runpy.run_path(str(ROOT / 'Scripts/PlanConfiguration/sanitize.py'),
                     run_name='delivery_sanitizer_import')
sanitized = BASE['sanitized']
validate = BASE['validate']
collect_identities = BASE['collect_identities']
atomic_write = BASE['atomic_write']
digest = BASE['digest']
TEXT = {'.log', '.json', '.jsonl', '.html', '.txt', '.xml', '.py', '.md', '.csv', '.ps1', '.mjs'}
QUERY_KEYS = (r'(?:key|session|session_?id|access_?token|refresh_?token|id_?token|'
              r'api[_-]?key|token|auth|authorization|client_secret|password|secret|'
              r'sig|signature|x-amz-[a-z-]+|x-goog-[a-z-]+)')
# Include HTML, JSON-escaped and percent-encoded separators without decoding or
# reserializing the document (which would change source lines and diagnostics).
QUERY = re.compile(r'(?i)((?:[?&]|&amp;|&#(?:38|x26);|\\u00(?:26|3f)|%(?:26|3f))'
                   + QUERY_KEYS + r'(?:=|%3d))'
                   r'((?:(?!&amp;|&#(?:38|x26);|\\u0026|%26)[^\s&"\'<>\\#])+)')
LOCAL_URL = re.compile(r'(?i)("localServerUrl"\s*:\s*")((?:\\.|[^"\\])*)(")')
USERINFO = re.compile(r'(?i)(https?://)([^/\s"<>:@]+:[^/\s"<>@]+)(@)')
PLACEHOLDER = re.compile(r'<(?:ACCOUNT|USER|MACHINE|MACHINE_ID|LOCAL_HOST|LOCAL_INTERFACE|'
                         r'UE_CRASH_SESSION|UE_SESSION|UE_INSTANCE_[0-9a-f]+|REDACTED_[A-Z_]+)>')
CRITICAL = {'status', 'state', 'result', 'test', 'testname', 'test_name',
            'fulltestpath', 'testdisplayname', 'outcome', 'code'}
XML_CRITICAL = {'tests', 'failures', 'errors', 'disabled', 'skipped', 'time',
                'timestamp', 'name', 'classname', 'status', 'result', 'type'}


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], stderr=subprocess.PIPE)


def untracked():
    return sorted(p for p in git('ls-files', '--others', '--exclude-standard', '-z', '--', 'Evidence')
                  .decode('utf-8').split('\0') if p)


def safe_path(relative, area='Evidence'):
    path = ROOT / relative
    if not path.resolve().is_relative_to((ROOT / area).resolve()):
        raise ValueError('Path escaped allowed area')
    if any(p.is_symlink() for p in [path, *path.parents] if p != ROOT.parent):
        raise ValueError('Symbolic link in evidence/backup path')
    return path


def redacted(value):
    value = unquote(value).replace('&lt;', '<').replace('&gt;', '>')
    return not value or value.startswith('<REDACTED_')


def extras(text):
    counts = Counter()
    def query(m):
        if redacted(m.group(2)):
            return m.group(0)
        counts['sensitive-url-query'] += 1
        return m.group(1) + '<REDACTED_URL_QUERY>'
    def local(m):
        if redacted(m.group(2)):
            return m.group(0)
        counts['local-server-url'] += 1
        return m.group(1) + '<REDACTED_LOCAL_URL>' + m.group(3)
    def userinfo(m):
        if redacted(m.group(2)):
            return m.group(0)
        counts['url-userinfo'] += 1
        return m.group(1) + '<REDACTED_USERINFO>' + m.group(3)
    text = QUERY.sub(query, text)
    text = LOCAL_URL.sub(local, text)
    text = USERINFO.sub(userinfo, text)
    return text, counts


def structured_equal(original, public, key=''):
    if type(original) is not type(public):
        raise ValueError('JSON type changed')
    if isinstance(original, dict):
        if original.keys() != public.keys():
            raise ValueError('JSON keys changed')
        for name in original:
            structured_equal(original[name], public[name], name.lower())
    elif isinstance(original, list):
        if len(original) != len(public):
            raise ValueError('JSON array length changed')
        for a, b in zip(original, public):
            structured_equal(a, b, key)
    elif not isinstance(original, str) or key in CRITICAL:
        if original != public:
            raise ValueError('JSON result or non-string value changed')


def xml_equal(original, public):
    a, b = ET.fromstring(original), ET.fromstring(public)
    aa, bb = list(a.iter()), list(b.iter())
    if len(aa) != len(bb):
        raise ValueError('XML element count changed')
    for x, y in zip(aa, bb):
        if x.tag != y.tag or x.attrib.keys() != y.attrib.keys() or len(x) != len(y):
            raise ValueError('XML structure changed')
        for key in x.attrib:
            if (key in XML_CRITICAL or re.fullmatch(r'[-+]?\d+(?:\.\d+)?', x.attrib[key])) and x.attrib[key] != y.attrib[key]:
                raise ValueError('XML test identity/result/count changed')


def validate_all(path, original, public):
    validate(path, original, public)
    if re.findall(r'\r\n|\r|\n', original) != re.findall(r'\r\n|\r|\n', public):
        raise ValueError('Line endings changed')
    if path.suffix == '.json':
        structured_equal(json.loads(original), json.loads(public))
    elif path.suffix == '.jsonl':
        for a, b in zip(original.splitlines(), public.splitlines()):
            if a.strip():
                structured_equal(json.loads(a), json.loads(b))
    elif path.suffix == '.xml':
        xml_equal(original, public)
    # Base validation checks known token formats. Idempotence additionally checks
    # credential fields, identities, URL queries and invocation/localServerUrl.
    residual = secret_findings(public)
    if residual:
        raise ValueError('Remaining sensitive categories: ' + ','.join(sorted(residual)))


def secret_findings(text):
    result = Counter()
    for category, pattern in BASE['TOKENS'].items():
        result[category] += len(re.findall(pattern, text))
    for pattern, category in [(QUERY, 'sensitive-url-query'), (LOCAL_URL, 'local-server-url'),
                              (USERINFO, 'url-userinfo'), (BASE['JSON_AUTH'], 'credential-field'),
                              (BASE['TEXT_AUTH'], 'credential-field'), (BASE['BEARER'], 'credential-field')]:
        result[category] += sum(not redacted(m.group(2)) for m in pattern.finditer(text))
    return Counter({k: v for k, v in result.items() if v})


def transform(path, raw, identities):
    old = raw.decode('utf-8-sig')
    new, counts = sanitized(path, old, identities)
    new, more = extras(new)
    counts = Counter(counts) + more
    if path.suffix == '.xml':
        # Escape only sanitizer placeholders, never serialize or drop XML nodes.
        new = PLACEHOLDER.sub(lambda m: '&lt;' + m.group(0)[1:-1] + '&gt;', new)
    validate_all(path, old, new)
    result = (b'\xef\xbb\xbf' if raw.startswith(b'\xef\xbb\xbf') else b'') + new.encode('utf-8')
    return result, dict(sorted(counts.items()))


def self_test():
    examples = [
        'https://example.invalid/?key=sample-value&session=sample-session',
        r'https://example.invalid/?sessionId=sample-value\u0026token=sample-token',
        'https://example.invalid/?key=sample-value&amp;session_id=sample-session',
        'https://example.invalid/%3Fkey%3Dsample-value%26session%3Dsample-session',
        '{"localServerUrl":"http://127.0.0.1:1234/private?key=sample-value","failed":2,"pass":false}',
        'https://sample-user:sample-password@example.invalid/path',
    ]
    for item in examples:
        public, counts = extras(item)
        assert counts and not secret_findings(public)
        assert extras(public)[0] == public
    xml = '<testsuites tests="2" failures="1"><testcase name="sample" file="C:/Users/SampleUser/file.cpp"><failure>retained</failure></testcase></testsuites>\n'
    raw, _ = transform(Path('sample.xml'), xml.encode(), {})
    xml_equal(xml, raw.decode())
    assert '&lt;USER&gt;' in raw.decode()
    for broken in ['{"pass":true,"failed":2}', '{"pass":false,"failed":0}']:
        try:
            structured_equal(json.loads('{"pass":false,"failed":2}'), json.loads(broken))
        except ValueError:
            pass
        else:
            raise AssertionError('Changed outcome was accepted')
    print(json.dumps({'self_test': 'PASS', 'cases': 9}))


def verify_manifest(report_path):
    report = json.loads(report_path.read_text(encoding='utf-8'))
    changed = 0
    for entry in report['files']:
        path = safe_path(entry['path'])
        data = path.read_bytes()
        if digest(data) != entry['public_sha256']:
            raise ValueError('Public digest mismatch: ' + entry['path'])
        if entry['changed']:
            changed += 1
            original = safe_path(entry['raw_backup'], 'Saved/Delivery/raw-evidence').read_bytes()
            if digest(original) != entry['original_sha256']:
                raise ValueError('Raw backup digest mismatch: ' + entry['path'])
        else:
            original = data
            if entry['original_sha256'] != entry['public_sha256']:
                raise ValueError('Unchanged digest mismatch')
        if path.suffix in TEXT:
            validate_all(path, original.decode('utf-8-sig'), data.decode('utf-8-sig'))
    print(json.dumps({'mode': 'verify-manifest', 'files': len(report['files']),
                      'raw_backups_verified': changed, 'remaining_secret': 0,
                      'all_public_hashes_verified': True}))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--date', default=datetime.now().strftime('%Y%m%d'))
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--images-reviewed', action='store_true',
                        help='Attest that every selected screenshot was visually reviewed for private information.')
    parser.add_argument('--verify-manifest', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not re.fullmatch(r'\d{8}', args.date):
        raise ValueError('Expected YYYYMMDD date')
    report_relative = 'Evidence/Delivery/' + args.date + '/sanitization.json'
    report_path = safe_path(report_relative)
    if args.verify_manifest:
        return verify_manifest(report_path)
    selected = [p for p in untracked() if p != report_relative]
    if not selected:
        raise ValueError('No untracked Evidence files')
    if args.apply and report_path.exists():
        raise ValueError('Manifest already exists; verify it instead of replacing its original hash trail')
    inputs = []
    originals = {}
    images = []
    for relative in selected:
        path = safe_path(relative)
        if not path.is_file() or path.suffix not in TEXT | {'.png'}:
            raise ValueError('Unsupported evidence file: ' + relative)
        raw = path.read_bytes()
        originals[relative] = raw
        if path.suffix in TEXT:
            inputs.append((path, raw, raw.decode('utf-8-sig')))
        else:
            if not (raw.startswith(b'\x89PNG\r\n\x1a\n') or raw.startswith(b'\xff\xd8\xff')):
                raise ValueError('Unsupported screenshot image signature')
            images.append(relative)
        if path.name == 'run.json' and any(p.startswith('verification-') for p in path.parts):
            if args.apply and 'pass' not in json.loads(raw.decode('utf-8-sig')):
                raise ValueError('Verification run is still writing: ' + relative)
    if args.apply and images and not args.images_reviewed:
        raise ValueError('Visual review required for selected screenshots')
    identities = collect_identities(inputs)
    prepared, totals = [], Counter()
    image_formats = Counter('PNG' if originals[p].startswith(b'\x89PNG\r\n\x1a\n') else 'JPEG' for p in images)
    for relative in selected:
        path, original = safe_path(relative), originals[relative]
        public, counts = transform(path, original, identities) if path.suffix in TEXT else (original, {})
        entry = {'path': relative, 'changed': original != public,
                 'original_sha256': digest(original), 'public_sha256': digest(public),
                 'original_bytes': len(original), 'public_bytes': len(public), 'categories': counts}
        if path.suffix in TEXT:
            entry['lines'] = len(original.decode('utf-8-sig').splitlines())
        totals.update(counts)
        prepared.append((path, original, public, entry))
    summary = {'mode': 'apply' if args.apply else 'audit', 'files': len(prepared),
               'files_changed' if args.apply else 'files_to_change': sum(x[3]['changed'] for x in prepared),
               'original_bytes': sum(len(x[1]) for x in prepared), 'image_files': len(images), 'image_formats': dict(image_formats),
               'xml_files': sum(x[0].suffix == '.xml' for x in prepared),
               'remaining_secret': 0, 'categories': dict(sorted(totals.items()))}
    if args.apply:
        # Snapshot recheck makes concurrent writes/staging fail before any write.
        if selected != [p for p in untracked() if p != report_relative]:
            raise ValueError('Untracked Evidence selection changed during audit')
        for path, original, _, _ in prepared:
            if path.read_bytes() != original:
                raise ValueError('Evidence changed during audit: ' + path.name)
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        backup_root = 'Saved/Delivery/raw-evidence/' + stamp
        backup_probe = backup_root + '/check.txt'
        if subprocess.run(['git', '-C', str(ROOT), 'check-ignore', '-q', backup_probe]).returncode:
            raise ValueError('Raw backup is not ignored by Git')
        # Back up and verify every changed original before publishing any file.
        for _, original, _, entry in prepared:
            if entry['changed']:
                entry['raw_backup'] = backup_root + '/' + entry['path']
                backup = safe_path(entry['raw_backup'], 'Saved/Delivery/raw-evidence')
                if backup.exists():
                    raise ValueError('Raw backup already exists')
                atomic_write(backup, original)
                if digest(backup.read_bytes()) != entry['original_sha256']:
                    raise ValueError('Raw backup verification failed')
        for path, _, public, entry in prepared:
            if entry['changed']:
                atomic_write(path, public)
        report = {'policy_version': 1, 'created_at_utc': stamp,
                  'scope': 'Git-untracked Evidence files snapshotted immediately before apply',
                  'excluded': ['tracked historical Evidence', report_relative, 'all files outside Evidence'],
                  'raw_backup_root': backup_root, 'summary': summary,
                  'preserved': ['failed evidence', 'all lines and line endings', 'JSON keys and non-string values',
                                'test result and identity strings', 'XML hierarchy, test identities and numeric attributes',
                                'screenshot bytes', 'Mock business IDs'],
                  'image_review': {'count': len(images), 'actual_formats': dict(image_formats),
                                   'filename_extensions': dict(Counter(Path(p).suffix for p in images)), 'status': 'VISUALLY_REVIEWED_NO_PRIVATE_INFORMATION',
                                   'method': 'Direct image inspection; no image editing', 'paths': images},
                  'files': [x[3] for x in prepared]}
        atomic_write(report_path, (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode())
        verify_manifest(report_path)
        summary['manifest'] = report_relative
        summary['raw_backup_root'] = backup_root
    print(json.dumps(summary, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, AssertionError, ET.ParseError, subprocess.CalledProcessError) as exc:
        # Never echo a raw document, credential or parser error snippet.
        print('Evidence preparation stopped: ' + type(exc).__name__, file=sys.stderr)
        sys.exit(1)
