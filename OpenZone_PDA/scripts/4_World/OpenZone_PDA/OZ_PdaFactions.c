// Фракції.
//
// КПК їх НЕ ПРИДУМУЄ. На сталкерських серверах фракції вже є -- їх приносить
// окремий мод, і він же вирішує, хто до кого належить. Тому тут той самий
// договір, що вже двічі спрацював (квести, радіація): чужий мод підставляє
// постачальника, а КПК просто питає.
//
// Але й БЕЗ постачальника мод мусить працювати: тоді фракція гравця лежить у
// його ж файлі акаунта, і її ставить адмін або квестовий мод. Порожня
// означає «одинак» -- і це не помилка, а найчастіший стан у Зоні.
//
// Список фракцій (id, назва, колір) читається з Factions.json. Він потрібен
// навіть при живому постачальнику: постачальник каже, ХТО в якій фракції, а
// як її звати людською мовою й яким кольором малювати -- питання оформлення,
// і воно наше.

class OZ_Faction
{
    string Id          = "";
    string DisplayName = "";

    // Колір рядком "R G B", 0..255. Не число ARGB: у JSON його читає адмін,
    // а 4278219546 не каже нікому нічого.
    string Color = "200 200 200";
}

class OZ_FactionsConfig : OZ_ConfigBase
{
    ref array<ref OZ_Faction> Factions;

    override int LatestVersion()
    {
        return 1;
    }

    override void LoadDefaults()
    {
        Version  = LatestVersion();
        Factions = new array<ref OZ_Faction>();

        // Лор STALKER. Назви лишаються як є -- це імена з першоджерела, а не
        // текст інтерфейсу, і перекладати їх нема потреби.
        Add("loner",     "Loners",       "200 200 200");
        Add("duty",      "Duty",         "196  64  40");
        Add("freedom",   "Freedom",      " 96 176  72");
        Add("bandit",    "Bandits",      "150 120  70");
        Add("mercenary", "Mercenaries",  " 80 130 190");
        Add("military",  "Military",     "110 130  90");
        Add("monolith",  "Monolith",     "170 150 220");
        Add("ecologist", "Ecologists",   "230 200  90");
    }

    private void Add(string id, string name, string colour)
    {
        OZ_Faction f = new OZ_Faction();
        f.Id          = id;
        f.DisplayName = name;
        f.Color       = colour;
        Factions.Insert(f);
    }

    override bool Migrate(int from)
    {
        Version = LatestVersion();
        return true;
    }

    override void Validate(out int warnings)
    {
        warnings = 0;

        if (!Factions)
            Factions = new array<ref OZ_Faction>();

        for (int i = 0; i < Factions.Count(); i++)
        {
            if (Factions[i].Id == "")
            {
                OZ_Log.Warn("faction #" + i.ToString() + " has no Id - it will never match anybody");
                warnings++;
            }
        }
    }
}

// Договір для чужого мода. Успадковуєш, перекриваєш FactionOf, прив'язуєш
// одним рядком зі свого OnMissionStart:
//
//     OZ_Factions.Bind(new MyFactionProvider());
//
// Повертати треба id зі свого ж Factions.json. Незнайоме id КПК покаже як є
// -- краще чуже слово на екрані, ніж мовчазна підміна на «одинак».
class OZ_FactionProvider
{
    string FactionOf(PlayerBase player)
    {
        return "";
    }
}

class OZ_Factions
{
    private static ref OZ_FactionsConfig s_Cfg;
    private static ref OZ_FactionProvider s_Provider;

    static void Bind(OZ_FactionProvider provider)
    {
        s_Provider = provider;
        OZ_Log.Dbg("faction provider bound");
    }

    static bool HasProvider()
    {
        return s_Provider != null;
    }

    static void ServerLoad()
    {
        s_Cfg = new OZ_FactionsConfig();
        OZ_ConfigLoader<OZ_FactionsConfig>.Load(OZ_Const.PROFILE_DIR + "\\Factions.json", "factions", s_Cfg);
    }

    static int Count()
    {
        if (!s_Cfg || !s_Cfg.Factions)
            return 0;
        return s_Cfg.Factions.Count();
    }

    // Чия фракція. Спершу питаємо постачальника -- він знає краще за нас; і
    // лише якщо його немає або він мовчить, дивимось у файл акаунта.
    static string Of(PlayerBase player, string uid)
    {
        if (s_Provider && player)
        {
            string fromMod = s_Provider.FactionOf(player);
            if (fromMod != "")
                return fromMod;
        }

        if (uid == "")
            return "";

        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        return d.Faction;
    }

    static OZ_Faction Find(string id)
    {
        if (id == "" || !s_Cfg || !s_Cfg.Factions)
            return null;

        for (int i = 0; i < s_Cfg.Factions.Count(); i++)
        {
            if (s_Cfg.Factions[i].Id == id)
                return s_Cfg.Factions[i];
        }
        return null;
    }

    // Людська назва. Незнайоме id повертаємо ЯК Є: чуже слово на екрані
    // чесніше за мовчазну підміну на «одинак».
    static string NameOf(string id)
    {
        if (id == "")
            return "";

        OZ_Faction f = Find(id);
        if (f)
            return f.DisplayName;
        return id;
    }
}
