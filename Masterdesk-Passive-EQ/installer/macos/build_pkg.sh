
# Builds, tests, validates and packages Masterdesk Passive EQ on macOS, Windows and Linux.
#  - Every push / pull request: installers appear under the run's "Artifacts".
#  - Pushing a tag like v1.0.0: also publishes a GitHub Release with the installers.
name: Build plugin

on:
  push:
    branches: [ main, master ]
    tags: [ 'v*' ]
  pull_request:
  workflow_dispatch:

env:
  BUILD_TYPE: Release
  ART: build/PassiveMasterEQ_artefacts/Release
  NAME: Masterdesk Passive EQ

jobs:
  build:
    name: ${{ matrix.name }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - { name: macOS,   os: macos-14 }
          - { name: Windows, os: windows-2022 }
          - { name: Linux,   os: ubuntu-22.04 }

    steps:
      - uses: actions/checkout@v4

      # Works whether the project files are at the top of the repository or
      # inside a folder (e.g. "Masterdesk-Passive-EQ/") after a web upload.
      - name: Locate project
        shell: bash
        run: |
          if [ -f CMakeLists.txt ] && grep -q juce_add_plugin CMakeLists.txt; then
            echo "Project is at the repository root."
          else
            f=$(grep -rl --include=CMakeLists.txt "juce_add_plugin" . | head -1 || true)
            if [ -z "$f" ]; then
              echo "::error::Could not find the plugin project (CMakeLists.txt with juce_add_plugin). Upload the files from the zip."
              exit 1
            fi
            dir=$(dirname "$f")
            echo "Project found in $dir - using it."
            cp -R "$dir"/. .
          fi

      - name: Version
        id: ver
        shell: bash
        run: |
          v=$(sed -n 's/^project(PassiveMasterEQ VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
          echo "version=$v" >> "$GITHUB_OUTPUT"

      - name: Linux dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y libasound2-dev libjack-jackd2-dev libx11-dev libxext-dev libxrandr-dev \
            libxinerama-dev libxcursor-dev libxcomposite-dev libxrender-dev libfreetype-dev \
            libfontconfig1-dev libgl1-mesa-dev ninja-build

      - name: Cache JUCE download
        uses: actions/cache@v4
        with:
          path: build/_deps
          key: juce-8.0.10-${{ runner.os }}

      - name: Configure
        shell: bash
        run: |
          if [ "$RUNNER_OS" = "Windows" ]; then
            cmake -B build -G "Visual Studio 17 2022" -A x64 -DPPM_BUILD_TESTS=ON
          elif [ "$RUNNER_OS" = "Linux" ]; then
            cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DPPM_BUILD_TESTS=ON
          else
            cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DPPM_BUILD_TESTS=ON
          fi

      - name: Build
        run: cmake --build build --config ${{ env.BUILD_TYPE }} --parallel 4

      - name: Run tests (DSP model + processor + presets)
        shell: bash
        env:
          PPM_SNAPSHOT_DIR: ${{ github.workspace }}/screenshots
        run: ctest --test-dir build -C $BUILD_TYPE --output-on-failure

      - name: Upload UI screenshots
        uses: actions/upload-artifact@v4
        with:
          name: screenshots-${{ matrix.name }}
          path: screenshots
          if-no-files-found: ignore

      # --- third-party validation (reported, never blocks the installers) ------
      - name: pluginval (VST3)
        continue-on-error: true
        shell: bash
        run: |
          case "$RUNNER_OS" in
            macOS)   zip=pluginval_macOS.zip;   exe=pluginval.app/Contents/MacOS/pluginval ;;
            Windows) zip=pluginval_Windows.zip; exe=./pluginval.exe ;;
            Linux)   zip=pluginval_Linux.zip;   exe=./pluginval ;;
          esac
          curl -sSL -o pv.zip "https://github.com/Tracktion/pluginval/releases/latest/download/$zip"
          unzip -q -o pv.zip
          extra=""; [ "$RUNNER_OS" = "Linux" ] && extra="--skip-gui-tests"
          "$exe" --strictness-level 5 --validate-in-process $extra --output-dir pluginval-logs \
                 --validate "$ART/VST3/$NAME.vst3"

      - name: auval (Audio Unit)
        if: runner.os == 'macOS'
        continue-on-error: true
        run: |
          mkdir -p ~/Library/Audio/Plug-Ins/Components
          cp -R "$ART/AU/$NAME.component" ~/Library/Audio/Plug-Ins/Components/
          killall -9 AudioComponentRegistrar || true
          auval -v aufx Mdeq Bsmt

      # --- packaging ------------------------------------------------------------
      # Plain zip of the Mac plug-ins first, so you always get them even if the .pkg step has a problem
      - name: Zip macOS plug-ins
        if: runner.os == 'macOS'
        run: |
          mkdir -p dist
          for b in "$ART/VST3/$NAME.vst3" "$ART/AU/$NAME.component" "$ART/Standalone/$NAME.app"; do
            codesign --force --deep --sign - "$b"
          done
          (cd "$ART" && zip -qry "$GITHUB_WORKSPACE/dist/Masterdesk-Passive-EQ-${{ steps.ver.outputs.version }}-macOS-plugins.zip" \
              "VST3/$NAME.vst3" "AU/$NAME.component" "Standalone/$NAME.app")

      # The .pkg installer is built right here, so this workflow file is the only thing to keep up to date
      - name: Package macOS installer
        if: runner.os == 'macOS'
        continue-on-error: true
        run: |
          set -euo pipefail
          VERSION="${{ steps.ver.outputs.version }}"
          ID="com.basemental.masterdeskpassiveeq"
          STAGE="$(mktemp -d)"
          mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app" "$STAGE/pkgs" "$STAGE/resources" dist
          cp -R "$ART/VST3/$NAME.vst3"      "$STAGE/vst3/"
          cp -R "$ART/AU/$NAME.component"   "$STAGE/au/"
          cp -R "$ART/Standalone/$NAME.app" "$STAGE/app/"

          make_pkg () {   # <root> <id-suffix> <install-location>
            local root="$1" suffix="$2" location="$3" plist="$STAGE/$2.plist" i=0
            pkgbuild --analyze --root "$root" "$plist"
            # mark every bundle pkgbuild found as non-relocatable (the list may be empty: that's fine)
            while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
              /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
              /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
              i=$((i + 1))
            done
            echo "$suffix: $i bundle(s) marked non-relocatable"
            pkgbuild --root "$root" --component-plist "$plist" --identifier "$ID.$suffix" \
                     --version "$VERSION" --install-location "$location" "$STAGE/pkgs/$suffix.pkg"
          }
          make_pkg "$STAGE/vst3" vst3 "/Library/Audio/Plug-Ins/VST3"
          make_pkg "$STAGE/au"   au   "/Library/Audio/Plug-Ins/Components"
          make_pkg "$STAGE/app"  app  "/Applications"

          printf '%s\n' "Masterdesk Passive EQ by Basemental" "" \
            "Passive EQ + Vari-Mu compressor with VU, spectrum, vectorscope and LUFS metering." "" \
            "Choose what to install on the next screens:" \
            "  - VST3 plug-in   -> /Library/Audio/Plug-Ins/VST3" \
            "  - Audio Unit     -> /Library/Audio/Plug-Ins/Components   (Logic Pro, GarageBand)" \
            "  - Standalone app -> /Applications" "" \
            "Afterwards, rescan plug-ins in your DAW. Inside the plug-in, press HELP ? for a guide." \
            > "$STAGE/resources/welcome.txt"

          cat > "$STAGE/distribution.xml" <<XML
          <?xml version="1.0" encoding="utf-8"?>
          <installer-gui-script minSpecVersion="2">
            <title>$NAME $VERSION</title>
            <welcome file="welcome.txt" mime-type="text/plain"/>
            <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
            <domains enable_localSystem="true"/>
            <choices-outline><line choice="vst3"/><line choice="au"/><line choice="app"/></choices-outline>
            <choice id="vst3" title="VST3 plug-in" description="For Cubase, Ableton Live, Studio One, Reaper, FL Studio, Bitwig."><pkg-ref id="$ID.vst3"/></choice>
            <choice id="au" title="Audio Unit (AU)" description="For Logic Pro, GarageBand and other AU hosts."><pkg-ref id="$ID.au"/></choice>
            <choice id="app" title="Standalone app" description="Runs on its own without a DAW."><pkg-ref id="$ID.app"/></choice>
            <pkg-ref id="$ID.vst3" version="$VERSION">vst3.pkg</pkg-ref>
            <pkg-ref id="$ID.au" version="$VERSION">au.pkg</pkg-ref>
            <pkg-ref id="$ID.app" version="$VERSION">app.pkg</pkg-ref>
          </installer-gui-script>
          XML

          productbuild --distribution "$STAGE/distribution.xml" --package-path "$STAGE/pkgs" \
                       --resources "$STAGE/resources" "dist/Masterdesk-Passive-EQ-$VERSION-macOS.pkg"
          echo "Built dist/Masterdesk-Passive-EQ-$VERSION-macOS.pkg"

      - name: Package Windows installer
        if: runner.os == 'Windows'
        shell: pwsh
        run: |
          choco install innosetup -y --no-progress
          $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
          $art  = Resolve-Path "$env:ART"
          New-Item -ItemType Directory -Force dist | Out-Null
          $out  = Resolve-Path dist
          & $iscc "/DAppVersion=${{ steps.ver.outputs.version }}" "/DArtefacts=$art" "/DOutDir=$out" installer\windows\installer.iss
          Compress-Archive -Path "$art\VST3\$env:NAME.vst3" -DestinationPath "dist\Masterdesk-Passive-EQ-${{ steps.ver.outputs.version }}-Windows-VST3.zip"

      - name: Package Linux tarball
        if: runner.os == 'Linux'
        run: |
          pkg="Masterdesk-Passive-EQ-${{ steps.ver.outputs.version }}-Linux-x64"
          mkdir -p "dist/$pkg"
          cp -R "$ART/VST3/$NAME.vst3" "dist/$pkg/"
          cp "$ART/Standalone/$NAME" "dist/$pkg/"
          cp installer/linux/install.sh "dist/$pkg/" && chmod +x "dist/$pkg/install.sh"
          tar -C dist -czf "dist/$pkg.tar.gz" "$pkg"
          rm -rf "dist/$pkg"

      - name: Upload installers
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: Masterdesk-Passive-EQ-${{ matrix.name }}
          path: dist/*
          if-no-files-found: ignore

  release:
    name: Publish release
    if: startsWith(github.ref, 'refs/tags/v')
    needs: build
    runs-on: ubuntu-latest
    permissions:
      contents: write
    steps:
      - uses: actions/download-artifact@v4
        with:
          pattern: Masterdesk-Passive-EQ-*
          path: dist
          merge-multiple: true
      - uses: softprops/action-gh-release@v2
        with:
          files: dist/*
          generate_release_notes: true
