//#include "AudioEngine/AudioEngine.h"
//#include <iostream>
//#include <filesystem>
//#include <Windows.h>
//
//int main()
//{
//    AudioEngine engine;
//
//    std::cout << "Current Path : "
//        << std::filesystem::current_path()
//        << std::endl;
//
//    const char* songFile = "D:\\Music\\song.mp3";
//
//    if (!std::filesystem::exists(songFile))
//    {
//        std::cout << "song.mp3 NOT Found!" << std::endl;
//        return -1;
//    }
//
//    std::cout << "song.mp3 Found!" << std::endl;
//
//    if (!engine.Initialize())
//    {
//        std::cout << "Engine Initialize Failed!" << std::endl;
//        return -1;
//    }
//
//    if (!engine.LoadSong(songFile))
//    {
//        std::cout << "Song Load Failed!" << std::endl;
//        engine.Shutdown();
//        return -1;
//    }
//
//    engine.PlaySong();
//
//    std::cout << "Playback Started..." << std::endl;
//    std::cout << "Recording Started on Desktop..." << std::endl;
//    std::cout << "Press ESC to Exit and Save Recording..." << std::endl;
//
//    // === Recording Shuru karne ke liye comments hata diye hain ===
//    //engine.StartRecording("C:\\Users\\admin\\Desktop\\my_karaoke_song.wav");
//
//    while (engine.IsRunning())
//    {
//        engine.Process();
//
//        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
//            break;
//
//        Sleep(1);
//    }
//
//    // === Recording Rokne aur save karne ke liye comments hata diye hain ===
//    //engine.StopRecording();
//
//    engine.StopSong();
//    engine.Shutdown();
//
//    std::cout << "Recording Saved Successfully on Desktop!" << std::endl;
//
//    return 0;
//}

#include <QApplication>
#include "KaraokeDSPDashboard.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Main Dashboard Window launch ho rahi hai
    KaraokeDSPDashboard w;
    w.show();

    return app.exec();
}