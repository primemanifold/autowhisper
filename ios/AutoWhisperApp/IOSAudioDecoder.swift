import AVFoundation
import Foundation

struct IOSDecodedAudio: Equatable, Sendable {
    let samples: [Float]
    let sampleRate: Double
    let channelCount: Int
    let sourceURL: URL
}

enum IOSAudioDecoderError: LocalizedError, Equatable {
    case couldNotCreateBuffer
    case recordingTooLong
    case unsupportedFormat

    var errorDescription: String? {
        switch self {
        case .couldNotCreateBuffer:
            return "AutoWhisper could not allocate a buffer for the recorded audio."
        case .recordingTooLong:
            return "This recording is too long for the current iOS preview decoder. Try a shorter note."
        case .unsupportedFormat:
            return "AutoWhisper could not decode the recorded audio into floating-point PCM."
        }
    }
}

final class IOSAudioDecoder {
    private let maxFrameCount: AVAudioFramePosition

    init(maxFrameCount: AVAudioFramePosition = AVAudioFramePosition(16_000 * 60 * 10)) {
        self.maxFrameCount = maxFrameCount
    }

    func decode(url: URL) throws -> IOSDecodedAudio {
        let audioFile = try AVAudioFile(forReading: url)
        let format = audioFile.processingFormat
        guard audioFile.length <= maxFrameCount else {
            throw IOSAudioDecoderError.recordingTooLong
        }
        let frameCapacity = AVAudioFrameCount(audioFile.length)
        guard let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: frameCapacity) else {
            throw IOSAudioDecoderError.couldNotCreateBuffer
        }

        try audioFile.read(into: buffer)
        guard let floatChannelData = buffer.floatChannelData else {
            throw IOSAudioDecoderError.unsupportedFormat
        }

        let frameCount = Int(buffer.frameLength)
        let channelCount = max(1, Int(format.channelCount))
        var samples: [Float] = []
        samples.reserveCapacity(frameCount)

        for frame in 0..<frameCount {
            var mixedSample: Float = 0
            for channel in 0..<channelCount {
                mixedSample += floatChannelData[channel][frame]
            }
            samples.append(mixedSample / Float(channelCount))
        }

        return IOSDecodedAudio(
            samples: samples,
            sampleRate: format.sampleRate,
            channelCount: channelCount,
            sourceURL: url
        )
    }
}
