# Android cleartext WebSocket

The game intentionally connects to `ws://158.220.122.81:8089/game`.

Godot's Android export must include the Internet permission. It is enabled in `export_presets.cfg`. If a particular Android export template blocks cleartext traffic, enable a Gradle custom build and add this attribute to the generated `<application>` element in `android/build/AndroidManifest.xml`:

```xml
android:usesCleartextTraffic="true"
```

This is only needed because the classroom server deliberately uses `ws://` instead of `wss://`.

