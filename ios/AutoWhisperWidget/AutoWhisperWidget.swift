import SwiftUI
import WidgetKit

private let quickRecordURL = URL(string: "autowhisper://record")!

struct AutoWhisperWidgetEntry: TimelineEntry {
    let date: Date
}

struct AutoWhisperWidgetProvider: TimelineProvider {
    func placeholder(in context: Context) -> AutoWhisperWidgetEntry {
        AutoWhisperWidgetEntry(date: Date())
    }

    func getSnapshot(in context: Context, completion: @escaping (AutoWhisperWidgetEntry) -> Void) {
        completion(AutoWhisperWidgetEntry(date: Date()))
    }

    func getTimeline(in context: Context, completion: @escaping (Timeline<AutoWhisperWidgetEntry>) -> Void) {
        completion(Timeline(entries: [AutoWhisperWidgetEntry(date: Date())], policy: .never))
    }
}

struct AutoWhisperWidgetView: View {
    let entry: AutoWhisperWidgetEntry

    var body: some View {
        Link(destination: quickRecordURL) {
            VStack(alignment: .leading, spacing: 10) {
                Image(systemName: "waveform.circle.fill")
                    .font(.system(size: 30, weight: .semibold))
                    .foregroundStyle(.orange)
                Text("AutoWhisper")
                    .font(.headline.weight(.semibold))
                Text("Open AutoWhisper to record")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(2)
                Spacer(minLength: 0)
                Text("Local voice")
                    .font(.caption2.weight(.semibold))
                    .textCase(.uppercase)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .padding()
        }
        .widgetURL(quickRecordURL)
    }
}

struct AutoWhisperWidget: Widget {
    let kind = "AutoWhisperWidget"

    var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: AutoWhisperWidgetProvider()) { entry in
            AutoWhisperWidgetView(entry: entry)
        }
        .configurationDisplayName("Quick Record")
        .description("Open AutoWhisper to record a local voice note in the foreground app.")
        .supportedFamilies([.systemSmall])
    }
}

@main
struct AutoWhisperWidgetBundle: WidgetBundle {
    var body: some Widget {
        AutoWhisperWidget()
    }
}
