// Id сторінки -> її клієнтський клас.
//
// ЄДИНЕ місце, де це знання живе. Реєстр сторінок у ядрі знає, що сторінка
// існує і хто відповідає на сервері; хто малює її на клієнті -- знає лише
// цей файл.
//
// Чужий мод, що приносить свою сторінку, кладе тут свою пару через Add() зі
// свого MissionGameplay.OnInit. Тому це не switch у меню: switch довелось би
// правити щоразу, коли з'являється чужа сторінка, тобто ніколи не правити.

class OZ_PdaPageFactory
{
    private static ref map<string, typename> s_Map;

    private static void Ensure()
    {
        if (s_Map)
            return;

        s_Map = new map<string, typename>();
        Add(OZ_PdaConst.PAGE_DEVICE, OZ_PdaPageDevice);
        Add(OZ_PdaConst.PAGE_QUESTS, OZ_PdaPageQuests);
        Add(OZ_PdaConst.PAGE_CONTACTS, OZ_PdaPageContacts);
        Add(OZ_PdaConst.PAGE_NOTES, OZ_PdaPageNotes);
    }

    static void Add(string pageId, typename pageClass)
    {
        Ensure();
        s_Map.Set(pageId, pageClass);
    }

    static bool Has(string pageId)
    {
        Ensure();
        return s_Map.Contains(pageId);
    }

    static OZ_PdaPage Make(string pageId)
    {
        Ensure();

        if (!s_Map.Contains(pageId))
        {
            // Сервер оголосив сторінку, якої клієнт малювати не вміє. Це не
            // падіння: вкладки просто не буде, а в лозі буде причина.
            OZ_Log.Warn("no client page class for \"" + pageId + "\" - tab skipped");
            return null;
        }

        OZ_PdaPage p;
        if (!Class.CastTo(p, s_Map.Get(pageId).Spawn()))
        {
            OZ_Log.Error("page class for \"" + pageId + "\" is not an OZ_PdaPage");
            return null;
        }
        return p;
    }

    // Одна літера для стрічки вкладок, поки немає власного imageset.
    static string Glyph(string pageId)
    {
        if (pageId == OZ_PdaConst.PAGE_DEVICE) return "D";
        if (pageId == OZ_PdaConst.PAGE_QUESTS) return "J";
        if (pageId == "map")      return "M";
        if (pageId == OZ_PdaConst.PAGE_CONTACTS) return "C";
        if (pageId == "chat")     return "@";
        if (pageId == "radio")    return "R";
        if (pageId == OZ_PdaConst.PAGE_NOTES) return "N";
        return "?";
    }
}
