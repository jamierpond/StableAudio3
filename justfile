set shell := ["bash", "-euo", "pipefail", "-c"]

bundle_id := "ai.tamber.stableaudio3"

# Jamie's free Personal Team. eacp's AppleSetup.cmake forces its own team into
# the cache for iOS, so the team goes on the xcodebuild line, which wins.
team := "DN5WMFL5W5"

# Build the desktop app (Release). EACP=$HOME/eacp builds against a local eacp.
build:
    [[ -f build/CMakeCache.txt ]] || cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DEACP_UNITY_BUILD=OFF ${EACP:+-DCPM_eacp_SOURCE=$EACP}
    cmake --build build

# Build and run on a simulator (by name).
ios-sim sim="iPhone 17 Pro":
    [[ -f build-ios-sim/CMakeCache.txt ]] || cmake -G Xcode -B build-ios-sim -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO -DEACP_UNITY_BUILD=OFF ${EACP:+-DCPM_eacp_SOURCE=$EACP}
    cmake --build build-ios-sim --config Release --target StableAudio3iOS -- -sdk iphonesimulator
    xcrun simctl boot "{{sim}}" 2>/dev/null || true
    open -a Simulator
    xcrun simctl install "{{sim}}" build-ios-sim/iOS/Release-iphonesimulator/StableAudio3iOS.app
    xcrun simctl terminate "{{sim}}" {{bundle_id}} 2>/dev/null || true
    xcrun simctl launch --console-pty "{{sim}}" {{bundle_id}}

# Build, sign, install and run on a phone (devicectl name or UDID).
ios device="pond":
    [[ -f build-ios/CMakeCache.txt ]] || cmake -G Xcode -B build-ios -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic -DEACP_UNITY_BUILD=OFF ${EACP:+-DCPM_eacp_SOURCE=$EACP}
    cmake --build build-ios --config Release --target StableAudio3iOS -- -sdk iphoneos -allowProvisioningUpdates -allowProvisioningDeviceRegistration DEVELOPMENT_TEAM={{team}} CODE_SIGN_IDENTITY="Apple Development"
    xcrun devicectl device install app --device "{{device}}" build-ios/iOS/Release-iphoneos/StableAudio3iOS.app
    xcrun devicectl device process launch --terminate-existing --console --device "{{device}}" {{bundle_id}}

# List phones devicectl can see.
devices:
    xcrun devicectl list devices

# Build and run the SA3 tests.
test: build
    ctest --test-dir build -R '^SA3' --output-on-failure
