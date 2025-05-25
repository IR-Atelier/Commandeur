#include <thread>
#include <portaudio.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <math.h>

#include <cstring>
#include <vosk_api.h>

using namespace std;

class AudioDevice {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual ~AudioDevice() {}
};

class Microphone : public AudioDevice {
protected:
    PaStream* stream;
public:
    void start() override {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            cerr << "PortAudio init error: " << Pa_GetErrorText(err) << endl;
            return;
        }

        err = Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, 16000, 512, nullptr, nullptr);
        if (err != paNoError) {
            cerr << "PortAudio open stream error: " << Pa_GetErrorText(err) << endl;
            return;
        }

        err = Pa_StartStream(stream);
        if (err != paNoError) {
            cerr << "PortAudio start stream error: " << Pa_GetErrorText(err) << endl;
        }
    }

    void stop() override {
        if (stream) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
        }
        Pa_Terminate();
    }
};

class SpeechRecognizer {
    VoskModel* model;
    VoskRecognizer* recognizer;
public:
    SpeechRecognizer(const char* model_path = "vosk-model-small-en-us-0.15", float sample_rate = 16000.0f) {
        model = vosk_model_new(model_path);
        if (!model) {
            cerr << "Failed to load Vosk model at: " << model_path << endl;
            return;
        }

        recognizer = vosk_recognizer_new(model, sample_rate);
        if (!recognizer) {
            cerr << "Failed to create Vosk recognizer" << endl;
        }
    }

    ~SpeechRecognizer() {
        if (recognizer) vosk_recognizer_free(recognizer);
        if (model) vosk_model_free(model);
    }

    bool acceptWaveform(const short* data, int length) {
        if (!recognizer) return false;
        return vosk_recognizer_accept_waveform(recognizer, (const char*)data, length * sizeof(short));
    }

    const char* getFinalResult() {
        if (!recognizer) return "{}";
        return vosk_recognizer_final_result(recognizer);
    }
};

class VoiceDetector : public Microphone {
    vector<float> data;
public:
    void record() {
        if (!stream) return;

        float buf[512];
        bool speaking = false;
        double last_speaking = Pa_GetStreamTime(stream);

        while (true) {
            PaError err = Pa_ReadStream(stream, buf, 512);
            if (err != paNoError) {
                cerr << "PortAudio read error: " << Pa_GetErrorText(err) << endl;
                break;
            }

            float level = 0;
            for (float f : buf) level += fabs(f);

            if (level / 512 > 0.01f) {
                if (!speaking) cout << "Speaking..." << endl;
                speaking = true;
                last_speaking = Pa_GetStreamTime(stream);
                data.insert(data.end(), buf, buf + 512);
            }
            else if (speaking && (Pa_GetStreamTime(stream) - last_speaking) >= 1.0) {
                break;
            }
        }
    }

    void play() {
        if (data.empty()) return;

        cout << "Playing..." << endl;
        PaStream* out;
        PaError err = Pa_OpenDefaultStream(&out, 0, 1, paFloat32, 16000, 512, nullptr, nullptr);
        if (err != paNoError) {
            cerr << "PortAudio output error: " << Pa_GetErrorText(err) << endl;
            return;
        }

        err = Pa_StartStream(out);
        if (err != paNoError) {
            cerr << "PortAudio start output error: " << Pa_GetErrorText(err) << endl;
            Pa_CloseStream(out);
            return;
        }

        for (size_t i = 0; i < data.size(); i += 512) {
            size_t remaining = data.size() - i;
            size_t writeSize = min<size_t>(512, remaining);
            err = Pa_WriteStream(out, &data[i], writeSize);
            if (err != paNoError) {
                cerr << "PortAudio write error: " << Pa_GetErrorText(err) << endl;
                break;
            }
        }

        Pa_StopStream(out);
        Pa_CloseStream(out);
        data.clear();
    }

    string convertAudioToText() {
        if (data.empty()) return "{}";

        SpeechRecognizer recognizer;
        vector<short> pcm(data.size());

        // Convert float to int16_t PCM
        for (size_t i = 0; i < data.size(); i++) {
            float sample = data[i];
            sample = max(-1.0f, min(1.0f, sample)); // Clamp
            pcm[i] = static_cast<short>(sample * 32767.0f);
        }

        const int chunk_size = 4000;
        for (size_t pos = 0; pos < pcm.size(); pos += chunk_size) {
            int remaining = pcm.size() - pos;
            int len = static_cast<int>(min(chunk_size, remaining));
            if (!recognizer.acceptWaveform(&pcm[pos], len)) {
                cerr << "Vosk recognition error" << endl;
            }
        }

        const char* result_json = recognizer.getFinalResult();
        return result_json ? string(result_json) : "{}";
    }
};

int main() {
    // Redirect stderr to NUL (Windows)
    FILE* dummyFile;
    if (freopen_s(&dummyFile, "NUL", "w", stderr) != 0) {
        cerr << "Failed to redirect stderr" << endl;
    }

    while (true) {
        VoiceDetector mic;
        mic.start();
        mic.record();
        mic.stop();

        string result = mic.convertAudioToText();
        cout << "Recognized Text JSON: " << result << endl;

        this_thread::sleep_for(chrono::seconds(1));

        mic.start();
        mic.play();
        mic.stop();

        
    }

    if (dummyFile) {
        fclose(dummyFile);
    }

    return 0;
}