#include <thread>
#include <conio.h> // For getch()

#include <portaudio.h>
#include <Windows.h>
#include <iostream>
#include <algorithm>
#include <map>
#include <vector>
#include <cmath>
#include <math.h>
#include <cstring>
#include <vosk_api.h>
#include <iomanip>
using namespace std;

// Function to display a goodbye splash screen when the program exits
// Function to set console color
void setColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// Function to center text
void printCentered(const string& text) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    int consoleWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int padding = (consoleWidth - text.length()) / 2;
    cout << setw(padding + text.length()) << text << endl;
}
void WelcomeSplashScreen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    for (int i = 1; i <= 4; i++) {
        SetConsoleTextAttribute(hConsole, i);  // Change text color

        cout << "\n\n\n\n\n";  // Vertical spacing for center effect
        printCentered("****************************************************************************");
        printCentered("**************        Creation of Love by IR Atelier          **************");
        printCentered("****************************************************************************");
        cout << endl;
        printCentered("-- THE NAME HAS SOUL --");

        cout << endl << endl;
        cout << "\t\t\t\t\t\t\tLOADING";
        for (int j = 0; j <= 4; j++) {
            cout << ".";
            Sleep(200);
        }
        cout << "\b\b\b\b\b\n";  // Erase dots

        system("cls || clear");
        SetConsoleTextAttribute(hConsole, 7);  // Reset color
    }
}

void showMenu() {
    setColor(14); // Yellow
    printCentered("----------------------------------------");
    printCentered("Main Menu ");
    printCentered("----------------------------------------");
    printCentered("1  View All Commands");
    printCentered("2  Voice Mode (Mic Commands)");
    printCentered("3  Exit");
    printCentered("----------------------------------------");
    cout << endl;
    setColor(7); // Reset
    printCentered("Please select an option: ");
}
void ExitSplashScreen() {
    system("cls || clear");  // Clear the screen before displaying exit message
    for (int i = 1; i <= 4; i++) {
        setColor(i);  // Change console text color

        cout << "\n\n\n\n\n\n\n\n\n\n\n\n";  // Move text to vertical center
        printCentered("****************************************************************************");
        printCentered("*************************        GOOD BYE          *************************");
        printCentered("****************************************************************************");
        cout<<"\t\t\t\t\t\t\tExiting";

        for (int j = 0; j <= 4; j++) {
            cout << ".";  // Display dots to simulate exiting
            Sleep(200);  // Pause for 200ms
        }

        cout << "\b\b\b\b\b\n";  // Erase last 5 characters (dots)
        Sleep(300);  // Small delay before next color change
        system("cls || clear");    // Clear the screen for next loop iteration
    }

    setColor(7);  // Reset color to default
    system("cls || clear");    // Clear the screen for next loop iteration
    exit(0);      // Exit the program
}


// Audio processing parameters
const float SPEECH_THRESHOLD = 0.03f;    // Volume level to detect speech
const float SILENCE_THRESHOLD = 0.01f;   // Volume level to detect silence
const double SILENCE_TIMEOUT = 1.5;      // Seconds of silence before stopping
const float MAX_AMPLIFICATION = 2.0f;    // Maximum gain for audio boost

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
        Pa_Initialize();
        Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, 16000, 512, nullptr, nullptr);
        Pa_StartStream(stream);
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
    SpeechRecognizer(const char* model_path = "model-en-inl", float sample_rate = 16000.0f) {
        model = vosk_model_new(model_path);
        if (!model) {
            cout << "Failed to load Vosk model at: " << model_path << endl;
            return;
        }

        recognizer = vosk_recognizer_new(model, sample_rate);
        if (!recognizer) {
            cout << "Failed to create Vosk recognizer" << endl;
        }
    }

    ~SpeechRecognizer() {
        if (recognizer) vosk_recognizer_free(recognizer);
        if (model) vosk_model_free(model);
    }

    bool acceptWaveform(const short* data, int length) {
        return vosk_recognizer_accept_waveform(recognizer, (const char*)data, length * sizeof(short));
    }

    const char* getFinalResult() {
        return vosk_recognizer_final_result(recognizer);
    }
};

class VoiceDetector : public Microphone {
    vector<float> data;

    // Audio enhancement functions
    void applyNoiseReduction(vector<float>& audio) {
        // Simple moving average filter
        for (size_t i = 1; i < audio.size(); i++) {
            audio[i] = 0.8f * audio[i] + 0.2f * audio[i - 1];
        }
    }

    void applyDynamicAmplification(vector<float>& audio) {
        // Calculate average level
        float avg_level = 0.0f;
        for (float sample : audio) {
            avg_level += abs(sample);
        }
        avg_level /= audio.size();

        // Calculate gain (capped at MAX_AMPLIFICATION)
        float gain = min(MAX_AMPLIFICATION, 0.3f / max(avg_level, 0.001f));

        // Apply gain with clipping protection
        for (float& sample : audio) {
            sample = max(-1.0f, min(1.0f, sample * gain));
        }
    }

public:
    void record() {
        float buf[512];
        bool speaking = false;
        double last_speaking = Pa_GetStreamTime(stream);

        cout << "Listening for speech..." << endl;

        while (true) {
            Pa_ReadStream(stream, buf, 512);

            // Calculate audio level (RMS)
            float sum_squares = 0.0f;
            for (float f : buf) sum_squares += f * f;
            float rms = sqrtf(sum_squares / 512);

            if (rms > SPEECH_THRESHOLD) {
                if (!speaking) cout << "Speech detected..." << endl;
                speaking = true;
                last_speaking = Pa_GetStreamTime(stream);
                data.insert(data.end(), buf, buf + 512);
            }
            else if (speaking) {
                // Keep recording during brief pauses
                data.insert(data.end(), buf, buf + 512);

                // Check for silence timeout
                if ((Pa_GetStreamTime(stream) - last_speaking) >= SILENCE_TIMEOUT) {
                    cout << "Silence detected, stopping recording" << endl;
                    break;
                }
            }
        }

        // Enhance audio before processing
        applyNoiseReduction(data);
        applyDynamicAmplification(data);
    }

    void play() {
        if (data.empty()) return;

        cout << "Playing back..." << endl;
        PaStream* out;
        Pa_OpenDefaultStream(&out, 0, 1, paFloat32, 16000, 512, nullptr, nullptr);
        Pa_StartStream(out);

        for (size_t i = 0; i < data.size(); i += 512) {
            size_t remaining = data.size() - i;
            size_t writeSize = min(static_cast<size_t>(512), remaining);
            Pa_WriteStream(out, &data[i], writeSize);
        }

        Pa_StopStream(out);
        Pa_CloseStream(out);
        data.clear();
    }

    string convertAudioToText() {
        if (data.empty()) return "{}";

        SpeechRecognizer recognizer;
        vector<short> pcm(data.size());

        // Enhanced conversion with dithering
        for (size_t i = 0; i < data.size(); i++) {
            float sample = max(-1.0f, min(1.0f, data[i]));
            // Add dithering to reduce quantization noise
            float dither = (rand() / (float)RAND_MAX) * 0.5f - 0.25f;
            pcm[i] = static_cast<short>(sample * 32767.0f + dither);
        }

        // Process in chunks with progress feedback
        const int chunk_size = 4000;
        for (size_t pos = 0; pos < pcm.size(); pos += chunk_size) {
            int remaining = static_cast<int>(pcm.size() - pos);
            int len = min(chunk_size, remaining);
            recognizer.acceptWaveform(&pcm[pos], len);

            // Show progress
            cout << "\rProcessing: " << (pos * 100 / pcm.size()) << "%" << flush;
        }
        cout << endl;

        return recognizer.getFinalResult();
    }
};


class Processor {
public:
    map<string, string> commandMap;  // {voice command, system command}


    Processor() {
        
        commandMap = {
            // System Controls
 {"shutdown", "shutdown /s /t 0"},
 {"restart", "shutdown /r /t 0"},
 {"log off", "shutdown /l"},
 {"lock", "rundll32.exe user32.dll,LockWorkStation"},
 {"sleep", "rundll32.exe powrprof.dll,SetSuspendState 0,1,0"},
 {"hibernate", "shutdown /h"},
 

 // Display Controls
 {"screen off", "nircmd.exe monitor off"},
 {"screen on", "nircmd.exe monitor on"},
 
 {"increase brightness", "nircmd.exe changebrightness 10"},
 {"decrease brightness", "nircmd.exe changebrightness -10"},

 // Power Management
 {"high performance", "powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"},
 {"balanced power", "powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e"},
 {"power saver", "powercfg /setactive a1841308-3541-4fab-bc81-f71556f20b4a"},

            // Applications
{"open calculator", "start calc"},
{"close calculator", "taskkill /IM Calculator.exe /F"},
{"open notepad", "start notepad"},
{"close notepad", "taskkill /IM notepad.exe /F"},
{"open wordpad", "start wordpad"},
{"close wordpad", "taskkill /IM wordpad.exe /F"},
{"open paint", "start mspaint"},
{"close paint", "taskkill /IM mspaint.exe /F"},
{"open command", "start cmd"},
{"close command", "taskkill /IM cmd.exe /F"},
{"open powershell", "start powershell"},
{"close powershell", "taskkill /IM powershell.exe /F"},
{"open explorer", "start explorer"},
{"close explorer", "taskkill /IM explorer.exe /F"},
{"open control panel", "start control"},
{"open task manager", "start taskmgr"},
{"open registry editor", "start regedit"},
{"open device manager", "start devmgmt.msc"},
{"open disk management", "start diskmgmt.msc"},
{"open system configuration", "start msconfig"},
{"open character map", "start charmap"},
{"open snipping tool", "start snippingtool"},
{"open sticky notes", "start stikynot"},
{"close sticky notes", "taskkill /IM stikynot.exe /F"},

// File Explorer
{"open my computer", "start explorer shell:MyComputerFolder"},
{"open network", "start explorer shell:NetworkFolder"},
{"open control panel", "start control"},
{"open settings", "start ms-settings:"},
{"open run dialog", "start run"},

// System Information
{"show system info", "systeminfo"},

{"show disk usage", "wmic logicaldisk get size,freespace,caption"},
{"show memory usage", "wmic os get freephysicalmemory,totalvisiblememorysize"},
// Microsoft Office
{"open word", "start winword"},
{"close word", "taskkill /IM winword.exe /F"},
{"open excel", "start excel"},
{"close excel", "taskkill /IM excel.exe /F"},
{"open powerpoint", "start powerpnt"},
{"close powerpoint", "taskkill /IM powerpnt.exe /F"},
{"open outlook", "start outlook"},
{"close outlook", "taskkill /IM outlook.exe /F"},
{"open onenote", "start onenote"},
{"close onenote", "taskkill /IM onenote.exe /F"},

// Web Browsers
{"open chrome", "start chrome"},
{"close chrome", "taskkill /IM chrome.exe /F"},
{"open edge", "start msedge"},
{"close edge", "taskkill /IM msedge.exe /F"},
{"open firefox", "start firefox"},
{"close firefox", "taskkill /IM firefox.exe /F"},
{"open internet explorer", "start iexplore"},
{"close internet explorer", "taskkill /IM iexplore.exe /F"},

// Communication
{"open teams", "start msteams"},
{"close teams", "taskkill /IM msteams.exe /F"},
{"open zoom", "start zoom"},
{"close zoom", "taskkill /IM zoom.exe /F"},
{"open skype", "start skype"},
{"close skype", "taskkill /IM skype.exe /F"},

// Media
{"open media player", "start wmplayer"},
{"close media player", "taskkill /IM wmplayer.exe /F"},
{"open vlc", "start vlc"},
{"close vlc", "taskkill /IM vlc.exe /F"},
{"open photos", "start photos"},
{"close photos", "taskkill /IM photos.exe /F"},

           // Media
            {"mute", "nircmd.exe mutesysvolume 1"},
            {"unmute", "nircmd.exe mutesysvolume 0"},
            {"volume up", "nircmd.exe changesysvolume 2000"},
            {"volume down", "nircmd.exe changesysvolume -2000"},

            // Network
            {"wifi on", "netsh interface set interface \"Wi-Fi\" enabled"},
            {"wifi off", "netsh interface set interface \"Wi-Fi\" disabled"},

     // Screenshot Utilities
 { "screenshot", "nircmd.exe savescreenshot \"C:\\Screenshots\\screenshot_$~$currdate.MMddyy$-$~$currtime.HHmmss$.png\"" },
 { "screenshot fullscreen", "nircmd.exe savescreenshotfull \"C:\\Screenshots\\full_$~$currdate.MMddyy$-$~$currtime.HHmmss$.png\"" },
 { "screenshot window", "nircmd.exe savescreenshotwin \"C:\\Screenshots\\window_$~$currdate.MMddyy$-$~$currtime.HHmmss$.png\"" },
 { "screenshot region", "nircmd.exe savescreenshotrect 0 0 800 600 \"C:\\Screenshots\\region_$~$currdate.MMddyy$-$~$currtime.HHmmss$.png\"" },

     // Clipboard Utilities
 { "clear clipboard", "nircmd.exe clipboard clear" },
 { "copy to clipboard", "nircmd.exe clipboard set \"$1\"" },
 { "save clipboard", "nircmd.exe clipboard save \"C:\\Clipboard\\clip_$~$currdate.MMddyy$.txt\"" },
 { "clipboard history", "nircmd.exe clipboard show" },

     // System Monitoring
 { "show cpu usage", "nircmd.exe sysmon showcpu" },
 { "show memory usage", "nircmd.exe sysmon showmemory" },
 { "show disk usage", "nircmd.exe sysmon showdisk" },
 { "show processes", "nircmd.exe sysmon showprocesses" },

     // Window Management
 { "minimize window", "nircmd.exe win min ititle \"$1\"" },
 { "maximize window", "nircmd.exe win max ititle \"$1\"" },
 { "close window", "nircmd.exe win close ititle \"$1\"" },
 
 { "move window", "nircmd.exe win move ititle \"$1\" 100 100 500 400" },

     // Audio Utilities
 
 { "mute application", "nircmd.exe muteappvolume \"$1\" 1" },
 { "unmute application", "nircmd.exe muteappvolume \"$1\" 0" },
{ "volume up application", "nircmd.exe changeappvolume \"$1\" 2000" },
 { "volume down application", "nircmd.exe changeappvolume \"$1\" -2000" },

     // Desktop Utilities
 { "show desktop", "nircmd.exe sendkeypress rwin+d" },
 { "empty recycle bin", "nircmd.exe emptybin" },
 
 //Folders
{ "open documents", "start %USERPROFILE%\\Documents" },
{ "open downloads", "start %USERPROFILE%\\Downloads" },
{ "open pictures", "start %USERPROFILE%\\Pictures" },
{ "open desktop", "start %USERPROFILE%\\Desktop" },
{ "open recent", "start shell:recent" },
{ "open root", "start \\" },

        };
    }
    string normalized;
    string getCommand(const string& voiceCommand) {
        normalized = voiceCommand;
        transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

        // Remove punctuation
        normalized.erase(remove_if(normalized.begin(), normalized.end(), ::ispunct), normalized.end());

        // Find matching command
        auto it = commandMap.find(normalized);
        if (it != commandMap.end()) {
            return it->second;
        }

        // Check for partial matches if exact not found
        for (const auto& pair : commandMap) {
            if (normalized.find(pair.first) != string::npos) {
                return pair.second;
            }
        }

        return "";  // Return empty string if no match
    }
};





int main() {
    WelcomeSplashScreen();  // Display splash screen

    FILE* dummyFile;
    if (freopen_s(&dummyFile, "NUL", "w", stderr) != 0)
        cout << "Failed to redirect stderr" << endl;

    char choice = '1';
    Processor processor;  // Create processor object for commands

    while (true) {
        system("cls || clear");  // Clear console
        showMenu();  // Display the menu

        choice = _getch();
        

        if (choice == '1') {
            system("cls || clear");
            setColor(11);  // Light cyan
            printCentered("------- Available Commands -------");
            cout << endl;

            // Display all commands from the commandMap
            for (const auto& pair : processor.commandMap) {
                printCentered(pair.first + " => " + pair.second);
            }


            setColor(7);
            printCentered("\nPress any key to go back to the menu...");
            _getch();  // Wait for key press
        }
        else if (choice == '2') {
            VoiceDetector mic;
            while (true) {
                mic.start();
                mic.record();
                mic.stop();

                std::string result = mic.convertAudioToText();
                printCentered("\nRecognized Text JSON: " + result + "\n");

               

                std::string systemCommand = processor.getCommand(result);
                if (!systemCommand.empty()) {
                    system(systemCommand.c_str());
                }
                if (processor.normalized.find("exit") != string::npos) {
                    printCentered("\nExit command detected. Closing...\n") ;
                    break;  // Exit the loop if "exit" found
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));

                mic.start();
                mic.play();
                mic.stop();

                setColor(7);  // Reset color (make sure this function is declared)
            }
        }
        else if (choice == '3') {
            setColor(10);  // Green
            
			ExitSplashScreen();  // Display exit splash screen
            setColor(7);  // Reset
            break;
        }
        else {
            setColor(12);  // Red
            printCentered("Invalid choice. Please try again.");
            setColor(7);
            Sleep(1000);
        }
    }

    if (dummyFile) {
        fclose(dummyFile);
    }

    return 0;
}
