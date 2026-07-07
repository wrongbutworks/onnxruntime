# Privacy

## Data Collection
The software may collect information about you and your use of the software and send it to Microsoft. Microsoft may use this information to provide services and improve our products and services. You may turn off the telemetry as described in the repository. There are also some features in the software that may enable you and Microsoft to collect data from users of your applications. If you use these features, you must comply with applicable law, including providing appropriate notices to users of your applications together with a copy of Microsoft's privacy statement. Our privacy statement is located at https://go.microsoft.com/fwlink/?LinkID=824704. You can learn more about data collection and use in the help documentation and our privacy statement. Your use of the software operates as your consent to these practices.

***

### Private Builds
On Windows, private builds compiled from source perform no data collection. On the non-Windows platforms, telemetry is enabled by default — including in builds compiled from source — so it is present unless you turn it off (see [Disabling Telemetry](#disabling-telemetry)).

### Official Builds
ONNX Runtime does not maintain any independent telemetry collection mechanisms outside of what is provided by the platforms it supports. However, where applicable, ONNX Runtime will take advantage of platform-supported telemetry systems to collect trace events with the goal of improving product quality.

Telemetry is turned **ON** by default in the official builds ([see here](../README.md#binaries)): on Windows it is implemented with the platform ETW provider, and on the non-Windows platforms — Linux, macOS, Android, and iOS — with the cross-platform 1DS telemetry provider (the standard build scripts enable the `--use_telemetry` build option for these). WebAssembly builds do not include telemetry. Data collection is implemented via 'Platform Telemetry' per vendor platform providers (see [telemetry.h](../onnxruntime/core/platform/telemetry.h)).

#### Technical Details

**Windows.** The Windows provider uses the [TraceLogging](https://docs.microsoft.com/en-us/windows/win32/tracelogging/trace-logging-about) API for its implementation. This enables ONNX Runtime trace events to be collected by the operating system, and based on user consent, this data may be periodically sent to Microsoft servers following GDPR and privacy regulations for anonymity and data access controls. Windows ML and onnxruntime C APIs allow Trace Logging to be turned on/off (see [API pages](../README.md#api-documentation) for details); there are equivalent APIs in the C#, Python, and Java language bindings as well.

**Non-Windows (Linux, macOS, Android, iOS).** These platforms use the cross-platform 1DS SDK ([cpp_client_telemetry](https://github.com/microsoft/cpp_client_telemetry)) to send the same trace events to Microsoft's telemetry backend over HTTPS. As on Windows, and based on user consent, this data is handled following GDPR and privacy regulations for anonymity and data access controls. WebAssembly builds are not supported and include no telemetry.

For the ways to disable telemetry, see the [Disabling Telemetry](#disabling-telemetry) section below.

### Disabling Telemetry

Telemetry can be disabled in any of these ways:

- **Don't build it in.** The telemetry provider is only compiled when configuring with `--use_telemetry`, so a build configured without it collects no data.
- **At runtime, via environment variable (non-Windows only).** Set `ORT_TELEMETRY_DISABLED=1` (also accepts `true`/`yes`/`on`/`y`, case-insensitive) before ONNX Runtime initializes. It is honored only by the non-Windows 1DS provider, where it stops the telemetry uploader from being created; it has **no effect on Windows**. The same variable is also honored by ONNX Runtime GenAI.
- **At runtime, via the API.** The C API (and the C#, Python, and Java bindings) expose calls to turn telemetry on/off. This is the way to control telemetry on **Windows**, whose ETW provider does not read `ORT_TELEMETRY_DISABLED`. (The Windows provider is passive regardless — events are only emitted while an external trace session is collecting.)
