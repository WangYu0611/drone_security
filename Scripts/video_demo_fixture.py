"""Local MOCK MP4 -> existing MediaMTX -> WebRTC fixture. No real UAV/backend.

Run with --ffmpeg <exe>; the default MediaMTX binary is the workstation's ignored
tools/MediaMTX/mediamtx.exe. Remove the stop file before a new run; write it to
stop this run. All generated files/process records are under Saved/VideoQA/demo.
The HTTP endpoint only supplies descriptors through the existing UE HTTP client.
"""
import argparse
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/VideoQA/demo"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ffmpeg", required=True)
    parser.add_argument("--mediamtx", default=str(ROOT / "tools/MediaMTX/mediamtx.exe"))
    parser.add_argument("--multi-feed", action="store_true", help="Add QA no-source and unavailable-source descriptors 4-6")
    args = parser.parse_args()
    for exe in [args.ffmpeg, args.mediamtx]:
        if not Path(exe).is_file():
            raise FileNotFoundError(exe)
    for port in [8554, 8889, 18989, 18997]:
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", port))
    OUT.mkdir(parents=True, exist_ok=True)
    if (OUT / "stop").exists():
        raise RuntimeError("Remove the prior stop file before starting a new fixture")
    files = []
    for drone in range(1, 4):
        target = OUT / f"uav-{drone:02d}-MOCK.mp4"
        vf = (f"hue=h={drone * 65},drawtext=fontfile='C\\:/Windows/Fonts/arial.ttf':"
              f"text='UAV-0{drone}  MOCK / LOCAL DEMO':fontsize=52:fontcolor=white:"
              "box=1:boxcolor=black@0.8:x=(w-tw)/2:y=60")
        subprocess.run([args.ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
                        "-i", "testsrc2=size=1280x720:rate=24", "-t", "8", "-vf", vf,
                        "-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p",
                        "-g", "24", "-bf", "0", "-an", str(target)], check=True,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        files.append({"drone": drone, "file": str(target), "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                      "video_url": f"http://127.0.0.1:8889/drone-{drone}"})
    (OUT / "sources.json").write_text(json.dumps(files, indent=2), encoding="utf-8")
    config = OUT / "mediamtx.yml"
    config.write_text((ROOT / "tools/MediaMTX/mediamtx.yml").read_text(encoding="utf-8") +
                      "\napi: true\napiAddress: 127.0.0.1:18997\n", encoding="utf-8")
    processes, logs = [], []

    def spawn(name, argv):
        log = open(OUT / f"{name}.log", "wb")
        logs.append(log)
        proc = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        processes.append(proc)
        return proc

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path.split("?")[0] != "/api/drones":
                self.send_error(404)
                return
            descriptors = [{"id": x["drone"], "name": "MOCK local MP4", "slot": x["drone"],
                            "video_url": x["video_url"], "ip": "127.0.0.1"} for x in files]
            if args.multi_feed:
                descriptors += [{"id": i, "name": "QA VISUAL STATE / unavailable source", "slot": i,
                                 "video_url": "http://127.0.0.1:8889/qa-unpublished" if i == 5 else "",
                                 "ip": "127.0.0.1"} for i in (4, 5, 6)]
            # Local fixture switch for empty-state acceptance, never production data.
            if (OUT / "empty").exists():
                descriptors = []
            body = json.dumps(descriptors).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    server = ThreadingHTTPServer(("127.0.0.1", 18989), Handler)
    try:
        spawn("mediamtx", [args.mediamtx, str(config)])
        time.sleep(2)
        for x in files:
            spawn(f"publisher-{x['drone']}", [args.ffmpeg, "-hide_banner", "-loglevel", "warning",
                  "-stream_loop", "-1", "-re", "-i", x["file"], "-c:v", "copy", "-an", "-f", "rtsp",
                  "-rtsp_transport", "tcp", f"rtsp://127.0.0.1:8554/drone-{x['drone']}"])
        (OUT / "processes.json").write_text(json.dumps([p.pid for p in processes]), encoding="utf-8")
        threading.Thread(target=server.serve_forever, daemon=True).start()
        while not (OUT / "stop").exists():
            if any(p.poll() is not None for p in processes):
                raise RuntimeError("A fixture media process stopped; inspect its log")
            time.sleep(0.5)
    finally:
        server.shutdown()
        server.server_close()
        for proc in reversed(processes):
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
        for log in logs:
            log.close()


if __name__ == "__main__":
    main()
