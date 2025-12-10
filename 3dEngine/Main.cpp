/*-------------------------------------------------
  Siv3D v0.6.16
  FreeCam + FPS + Move / Rotate / Scale Gizmo
  Backspace : FreeCam   /   Enter : FPS
  LMB       : Select / Drag
  W / E / R : Move / Rotate / Scale
 -------------------------------------------------*/
# include <Siv3D.hpp>
# include <functional>
# include <vector>
# include "Math.hpp"
# include "Cube.hpp"
# include "Gizmo.hpp"
# include "Camera.hpp"
# include "RenderUtils.hpp"
# include "Game.hpp"

void runGame()
{
    Game game;
    game.run();
}

void Main()
{
    HashTable<String, std::function<void()>> entryPoints = {
        { U"game", runGame },
    };

    const Array<String> args = System::GetArguments();
    const String key = (1 < args.size()) ? args[1] : U"game";
    if (auto it = entryPoints.find(key); it != entryPoints.end())
    {
        it->second();
    }
    else
    {
        Console << U"Unknown entry: " << key << U"\nAvailable entries:";
        for (const auto& [name, _] : entryPoints)
        {
            Console << U"\n - " << name;
        }
    }
}
