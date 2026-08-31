// Розкладка HUD -- ОСОБИСТА СПРАВА гравця.
//
// Позиції панелей живуть у профілі КЛІЄНТА ($profile на машині гравця), не
// на сервері: куди людині зручно дивитись -- знає лише вона, і сервер тут
// ні до чого. Частки екрана, не пікселі: та сама розкладка тримається на
// будь-якій роздільності.

class OZ_HudPanePos
{
    string Id = "";
    float  X  = 0;
    float  Y  = 0;
}

class OZ_HudLayoutFile
{
    int Version = 1;
    ref array<ref OZ_HudPanePos> Panes;

    void OZ_HudLayoutFile()
    {
        Panes = new array<ref OZ_HudPanePos>();
    }
}

class OZ_PdaHudLayout
{
    private static ref OZ_HudLayoutFile s_File;
    private static const string DIR  = "$profile:OpenZone";
    private static const string PATH = "$profile:OpenZone\\OZ_PDA_HudLayout.json";

    private static void LoadOnce()
    {
        if (s_File)
            return;

        s_File = new OZ_HudLayoutFile();

        if (FileExist(PATH))
            JsonFileLoader<OZ_HudLayoutFile>.JsonLoadFile(PATH, s_File);

        if (!s_File.Panes)
            s_File.Panes = new array<ref OZ_HudPanePos>();
    }

    // Чи є збережена позиція. Немає -- панель стоїть там, де поклав макет.
    static bool Get(string id, out float x, out float y)
    {
        LoadOnce();

        for (int i = 0; i < s_File.Panes.Count(); i++)
        {
            if (s_File.Panes[i].Id == id)
            {
                x = s_File.Panes[i].X;
                y = s_File.Panes[i].Y;
                return true;
            }
        }
        return false;
    }

    static void Put(string id, float x, float y)
    {
        LoadOnce();

        for (int i = 0; i < s_File.Panes.Count(); i++)
        {
            if (s_File.Panes[i].Id == id)
            {
                s_File.Panes[i].X = x;
                s_File.Panes[i].Y = y;
                return;
            }
        }

        OZ_HudPanePos p = new OZ_HudPanePos();
        p.Id = id;
        p.X  = x;
        p.Y  = y;
        s_File.Panes.Insert(p);
    }

    static void Forget(string id)
    {
        LoadOnce();

        for (int i = 0; i < s_File.Panes.Count(); i++)
        {
            if (s_File.Panes[i].Id == id)
            {
                s_File.Panes.Remove(i);
                return;
            }
        }
    }

    static void Save()
    {
        LoadOnce();

        if (!FileExist(DIR))
            MakeDirectory(DIR);

        JsonFileLoader<OZ_HudLayoutFile>.JsonSaveFile(PATH, s_File);
    }
}
