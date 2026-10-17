"""Sanitize public verification evidence; keep raw originals under ignored Saved/.

Run without arguments to audit (no writes), then --apply after verification has
stopped. Only Evidence/PlanConfiguration/verification-* is in scope; sealed
backend evidence and other evidence folders are never changed. Failed runs are
retained. Account/machine/UE-session identifiers are removed, while Mock request,
client, configuration, route, plan, mission and execution IDs remain intact.
"""

import argparse
from collections import Counter
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / 'Evidence/PlanConfiguration'
RAW = ROOT / 'Saved/PlanConfiguration/raw-evidence'
REPORT = EVIDENCE / 'sanitization.json'
VERSION = 1
TEXT_SUFFIXES = {'.log', '.json', '.jsonl', '.html', '.txt'}

# These detect credential formats, not arbitrary UUIDs or Mock session IDs.
TOKENS = {
    'jwt': r'eyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}',
    'github-token': r'(?:gh[pousr]_[A-Za-z0-9]{25,}|github_pat_[A-Za-z0-9_]{20,})',
    'google-api-key': r'AIza[0-9A-Za-z_-]{30,}',
    'openai-api-key': r'sk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{30,}',
    'aws-access-key': r'(?:AKIA|ASIA)[A-Z0-9]{16}',
    'slack-token': r'xox[baprs]-[A-Za-z0-9-]{20,}',
}
AUTH_KEY = r'(?:authorization|proxy-authorization|access_token|refresh_token|id_token|api_key|apikey|client_secret|secret|token|password|auth_password|aws_secret_access_key|ion_access_token)'
JSON_AUTH = re.compile(r'(?i)("' + AUTH_KEY + r'"\s*:\s*")((?:\\.|[^"\\])*)(")')
TEXT_AUTH = re.compile(r'(?i)(\b' + AUTH_KEY + r'\s*[=:]\s*)(?!["<])([^\s,;&"<>]+)')
BEARER = re.compile(r'(?i)(\bBearer\s+)([A-Za-z0-9_~+./=-]{8,})')
USER_PATH = re.compile(r'(?i)[A-Z]:[\\/]+Users[\\/]+[^\\/\s"<>]+')
REPO_PATH = re.compile(re.escape(str(ROOT)).replace(r'\\', r'[\\/]+') + r'(?:[\\/]+)?', re.I)

# Deliberately contextual: e.g. "Is Virtual User: 0" is a diagnostic, not an
# account. Generic session_id and instance_id belong to Mock business traffic.
IDENTITY_FIELDS = [
    ('account', re.compile(r'(?im)(\b(?:loginid|EpicAccountId|LoginId|AccountId)\s*[=:]\s*["\']?)([^\s"\'<>]+)'), '<ACCOUNT>'),
    ('machine', re.compile(r'(?im)(\bLogInit:\s*Computer:\s*)([^\r\n<>]+)'), '<MACHINE>'),
    ('account', re.compile(r'(?im)(\bLogInit:\s*User:\s*)([^\r\n<>]+)'), '<USER>'),
    ('machine-id', re.compile(r'(?im)(\bMachineId\s*[=:]\s*)([^\s"<>]+)'), '<MACHINE_ID>'),
    ('ue-session', re.compile(r'(?im)(\bSession CrashGUID\s*>[ \t]*)([^=\s<>][^\r\n<>]*)'), '<UE_CRASH_SESSION>'),
    ('ue-session', re.compile(r'(?im)(\|\s*Session:\s*)([A-Za-z0-9_-]{8,})'), '<UE_SESSION>'),
    ('machine-network', re.compile(r'(?im)(\bLocalHostAddr\s*=[ \t]*)([^\s<>][^\r\n<>]*)'), '<LOCAL_HOST>'),
    ('machine-network', re.compile(r"(?im)(\bAdded local interface ')([0-9.]+)(?=')"), '<LOCAL_INTERFACE>'),
]
REPORT_FIELDS = re.compile(r'("(?:deviceName|instanceName|instance|deviceInstance)"\s*:\s*")((?:\\.|[^"\\])*)(")')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_text(path):
    data = path.read_bytes()
    # Refuse undecodable input instead of losing bytes in a replacement decode.
    return data, data.decode('utf-8-sig')


def identifying(value):
    # Metadata sentinels and numbers are diagnostics, not identities. In
    # particular a global replacement of "0" would corrupt assertion results.
    return bool(value and not value.startswith('<') and
                value.casefold() not in {'none', 'null', 'unknown', 'true', 'false', 'default', 'anonymous'} and
                not value.isdecimal() and not re.fullmatch(r'[0{}-]+', value))


def collect_identities(inputs):
    """Link explicitly labeled UE identities consistently across report files."""
    identities = {}
    for path, _, text in inputs:
        for category, pattern, replacement in IDENTITY_FIELDS:
            if category == 'machine-network':
                continue  # Interface metadata must never rewrite HTTP endpoints.
            for match in pattern.finditer(text):
                value = match.group(2).strip()
                if identifying(value):
                    identities[value] = (category, replacement)
        if path.name in ('index.json', 'index.html'):
            for match in REPORT_FIELDS.finditer(text):
                value = json.loads('"' + match.group(2) + '"')
                if not identifying(value):
                    continue
                kind = 'ue-instance' if 'instance' in match.group(1).lower() else 'machine'
                replacement = '<UE_INSTANCE_' + digest(value.encode())[:12] + '>' if kind == 'ue-instance' else '<MACHINE>'
                # Keep an existing account/computer classification if this value
                # is also a report instanceName, rather than assigning two aliases.
                identities.setdefault(value, (kind, replacement))
    return identities


def sanitized(path, text, identities):
    counts = Counter()

    def replace(pattern, substitution, category):
        nonlocal text
        text, count = pattern.subn(substitution, text)
        if count:
            counts[category] += count

    replace(REPO_PATH, '', 'repository-path')
    # A path equal to ROOT alone is not currently emitted by verify.py. A
    # repository prefix becomes a relative path, retaining its source line.
    replace(USER_PATH, '<USER>', 'account-path')
    for category, pattern, replacement in IDENTITY_FIELDS:
        if category != 'machine-network':
            continue

        def network(match):
            value = match.group(2).strip()
            try:
                address = ipaddress.ip_address(value)
                if address.is_loopback or address.is_unspecified or address.is_multicast:
                    return match.group(0)
            except ValueError:
                pass  # A named workstation is identifying metadata as well.
            counts[category] += 1
            return match.group(1) + replacement

        text = pattern.sub(network, text)
    for value, (category, replacement) in sorted(identities.items(), key=lambda item: (-len(item[0]), item[0])):
        # Token boundaries avoid replacing a short account/host name inside a
        # diagnostic word. JSON escaped spelling is covered separately.
        spellings = {value, json.dumps(value, ensure_ascii=False)[1:-1]}
        for spelling in sorted(spellings, key=len, reverse=True):
            replace(re.compile(r'(?<![A-Za-z0-9_])' + re.escape(spelling) + r'(?![A-Za-z0-9_])'), replacement, category)
    for category, pattern in TOKENS.items():
        replace(re.compile(pattern), '<REDACTED_CREDENTIAL>', category)

    def auth(match):
        if not match.group(2) or match.group(2).startswith('<REDACTED_'):
            return match.group(0)
        counts['credential-field'] += 1
        return match.group(1) + '<REDACTED_CREDENTIAL>' + match.group(3)

    text = JSON_AUTH.sub(auth, text)
    replace(TEXT_AUTH, lambda m: m.group(1) + '<REDACTED_CREDENTIAL>', 'credential-field')
    replace(BEARER, lambda m: m.group(1) + '<REDACTED_CREDENTIAL>', 'credential-field')
    return text, dict(sorted(counts.items()))


def validate(path, original, public):
    if len(original.splitlines()) != len(public.splitlines()):
        raise ValueError('Sanitizer changed line count: ' + path.name)
    if path.suffix == '.json':
        json.loads(public)
    elif path.suffix == '.jsonl':
        for line in public.splitlines():
            if line.strip():
                json.loads(line)
    for category, pattern in TOKENS.items():
        if re.search(pattern, public):
            raise ValueError('Remaining ' + category + ' in ' + path.name)
    # Test result markers and assertion text are never filtered out. Public
    # absolute paths are normalized, but numeric diagnostics retain all values.
    for pattern in (r'Test Completed\. Result=\{[^}]+\} Name=\{[^}]*\} Path=\{[^}]+\}',
                    r'Found \d+ automation tests based on'):
        if re.findall(pattern, original) != re.findall(pattern, public):
            raise ValueError('Automation results changed: ' + path.name)


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='Back up originals and sanitize public copies; default is audit only.')
    args = parser.parse_args()
    inputs = []
    runs = sorted(EVIDENCE.glob('verification-*'))
    for run in runs:
        if not run.is_dir() or run.is_symlink():
            raise ValueError('Unexpected verification entry: ' + run.name)
        metadata = json.loads((run / 'run.json').read_text(encoding='utf-8-sig'))
        if args.apply and 'pass' not in metadata:
            raise ValueError('Verification is still running: ' + run.name)
        for path in sorted(run.rglob('*')):
            if path.is_symlink() or not path.resolve().is_relative_to(run.resolve()):
                raise ValueError('Evidence path escapes verification run: ' + path.name)
            if path.is_file() and path.suffix in TEXT_SUFFIXES:
                data, text = read_text(path)
                inputs.append((path, data, text))
    identities = collect_identities(inputs)
    prepared = []
    totals = Counter()
    for path, original_bytes, original_text in inputs:
        public_text, categories = sanitized(path, original_text, identities)
        validate(path, original_text, public_text)
        public_bytes = (b'\xef\xbb\xbf' if original_bytes.startswith(b'\xef\xbb\xbf') else b'') + public_text.encode('utf-8')
        relative = path.relative_to(EVIDENCE)
        changed = public_bytes != original_bytes
        record = {'path': path.relative_to(ROOT).as_posix(), 'changed': changed, 'categories': categories,
                  'input_sha256': digest(original_bytes), 'public_sha256': digest(public_bytes)}
        if changed:
            totals.update(categories)
            print(json.dumps({'path': record['path'], 'categories': categories}, ensure_ascii=False))
        prepared.append((path, relative, original_bytes, public_bytes, record))
    if args.apply:
        # Complete the audit before any write, then refuse files that changed
        # while being read. Final runs must have exited before --apply is used.
        for path, _, original, _, _ in prepared:
            if path.read_bytes() != original:
                raise ValueError('Evidence changed during audit: ' + path.name)
        for path, relative, original, public, record in prepared:
            if not record['changed']:
                continue
            backup = RAW / relative
            if backup.exists() and backup.read_bytes() != original:
                backup = RAW / 'revisions' / digest(original) / relative
            if not backup.exists():
                atomic_write(backup, original)
            if backup.read_bytes() != original:
                raise ValueError('Raw backup verification failed: ' + path.name)
            record['raw_backup'] = backup.relative_to(ROOT).as_posix()
            atomic_write(path, public)
        prior = json.loads(REPORT.read_text(encoding='utf-8')) if REPORT.exists() else {}
        records = {item['path']: item for item in prior.get('files', [])}
        for *_, record in prepared:
            # A repeated no-change audit retains the original backup/hash trail.
            if record['changed'] or record['path'] not in records:
                records[record['path']] = record
        report = {'policy_version': VERSION, 'scope': 'Evidence/PlanConfiguration/verification-*',
                  'excluded': ['Evidence/PlanConfiguration/backend/**'],
                  'raw_backup_root': RAW.relative_to(ROOT).as_posix(),
                  'preserved': ['failed runs', 'all log lines', 'assertions and test counts',
                                'HTTP diagnostic traces', 'Mock business IDs'],
                  'files': [records[key] for key in sorted(records)]}
        atomic_write(REPORT, (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode())
    print(json.dumps({'mode': 'apply' if args.apply else 'audit', 'runs': len(runs),
                      'files_scanned': len(prepared), 'files_changed' if args.apply else 'files_to_change':
                      sum(item[-1]['changed'] for item in prepared), 'categories': dict(sorted(totals.items()))}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, UnicodeError, ValueError) as exc:
        # Never include raw evidence snippets/credential values in diagnostics.
        print('Sanitization stopped: ' + type(exc).__name__ + ': ' + str(exc), file=sys.stderr)
        sys.exit(1)
