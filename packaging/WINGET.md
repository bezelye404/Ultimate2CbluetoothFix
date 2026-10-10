# winget packaging

Templates for the `microsoft/winget-pkgs` repository. Nothing here is built into the app.

At release time:
1. Publish the GitHub release with `Ultimate2CFixer.exe`; the version in `src/resource.rc`, `src/app.manifest` and these files must match the tag.
2. `certutil -hashfile Ultimate2CFixer.exe SHA256` and put the value into the installer file (or run `wingetcreate update bezelye404.Ultimate2CFixer --version <v> --urls <url> --submit`).
3. `winget validate --manifest packaging\winget` and test with `winget install --manifest packaging\winget` (needs `winget settings --enable LocalManifestFiles`).
4. Open the pull request against `microsoft/winget-pkgs` from the owner's account.

Drivers: ViGEmBus (`ViGEm.ViGEmBus`) and HidHide (`Nefarius.HidHide`) are listed as winget package dependencies, so `winget install` sets them up first. The app can still install them itself (publisher check) for people who download the exe by hand.

Folder layout in winget-pkgs: `manifests/b/bezelye404/Ultimate2CFixer/<version>/` with the three yaml files.
