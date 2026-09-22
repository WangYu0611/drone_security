# Optional video service: MediaMTX

MediaMTX is not needed for the Stage 1 Mock plan/route/execution workflow. It is only needed when configuring actual RTSP/WebRTC video sources.

The repository no longer distributes the third-party `mediamtx.exe` or its ZIP. Existing local copies are preserved but ignored by Git. Download from the publisher only:

- Official releases: https://github.com/bluenviron/mediamtx/releases
- Previously used version: https://github.com/bluenviron/mediamtx/releases/tag/v1.18.2
- v1.18.2 Windows amd64 ZIP SHA256: `945AB46C5FC6D2802AD18E2F1D7E49245CA5609657D85E310AA6EDA4CDD72EEC`

If video is required, verify the release archive against the publisher's checksum and extract `mediamtx.exe` here. Preserve this project's `mediamtx.yml`; do not replace it with the release's default configuration. Other versions have different checksums and need separate compatibility checks.

`start_demo_streams.ps1` is a legacy video demonstration requiring FFmpeg and your own recording paths. Its wrapper respects the Windows PowerShell execution policy. It does not change that policy or disable antivirus protection. Do not weaken an organization's policy to run it.

Removing bundled binaries is a distribution/dependency change, not proof that MediaMTX caused the reported Defender detection. The reported detection named the outer project ZIP only.
