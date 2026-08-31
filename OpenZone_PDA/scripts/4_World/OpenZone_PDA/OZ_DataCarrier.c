// Носій даних: флешка, чип, папка з логами -- що завгодно, що вставляється
// в КПК і щось у собі несе.
//
// Носій НЕ знає, що на ньому записано. Він везе СЕКЦІЇ: рід плюс корисне
// навантаження рядком JSON, а розбирає й малює це та сторінка, якій цей рід
// належить. Той самий принцип, що й у мосту: ядро возить конверти, а не читає
// листи.
//
// ЧОМУ СЕКЦІЇ, А НЕ ТРИ ПОЛЯ. Раніше тут стояли рівно три: мітки, нотатки,
// маршрут -- і формат сховища перелічував їх позиційно. Це означало, що чужий
// мод не міг покласти на носій НІЧОГО СВОГО: поля під нього немає, у форматі
// місця немає, а завантажувач чужий рід прямо викидав («читати його нема
// кому»). Тобто носій був відкритий на словах і закритий на ділі.
//
// Тепер секцій скільки завгодно, і рід -- звичайний рядок. Мод, що хоче
// зберігати своє, кличе OZ_Write("свій_рід", json, скільки_записів) і читає
// OZ_Read("свій_рід"). Ані КПК, ані цей файл про нього не знають.
//
// ВМІСТИМІСТЬ ЗАГАЛЬНА, В ЗАПИСАХ, і це друга половина тієї ж думки. Стелі
// «скільки міток» і «скільки нотаток» вимагали б від кожного носія знати про
// кожен майбутній рід, а від кожного нового роду -- прописатись у кожному
// носії. Замість цього носій має ОДНЕ число: скільки записів на нього влазить.
// Мітка -- запис, нотатка -- запис, точка маршруту -- запис, частота -- запис.
// Носій не питає, що це, і саме тому нічого не мусить знати наперед.

// Одна секція носія. Рід -- домовленість між тим, хто пише, і тим, хто читає;
// сюди він приходить рядком і тут рядком і лишається.
class OZ_CarrierSection
{
    string Kind    = "";
    string Payload = "";
    // Скільки записів коштує ця секція. Рахує ТОЙ, ХТО ПИШЕ: лише він знає,
    // що всередині його JSON. Носієві досить суми.
    int    Records = 0;
}

class OZ_DataCarrier_Base : ItemBase
{
    private ref array<ref OZ_CarrierSection> m_Sections;

    void OZ_DataCarrier_Base()
    {
        m_Sections = new array<ref OZ_CarrierSection>();
    }

    // ------------------------------------------------------------ читання

    private OZ_CarrierSection Find(string kind)
    {
        if (!m_Sections)
            return null;

        for (int i = 0; i < m_Sections.Count(); i++)
        {
            if (m_Sections[i].Kind == kind)
                return m_Sections[i];
        }
        return null;
    }

    string OZ_Read(string kind)
    {
        OZ_CarrierSection s = Find(kind);
        if (!s)
            return "";
        return s.Payload;
    }

    // -1 -- секції немає. Нуль -- секція є й порожня, що не те саме.
    int OZ_Records(string kind)
    {
        OZ_CarrierSection s = Find(kind);
        if (!s)
            return -1;
        return s.Records;
    }

    // Які роди тут узагалі лежать. Потрібно тому, хто малює носій, не знаючи
    // наперед, що на ньому.
    void OZ_Kinds(out array<string> kinds)
    {
        kinds = new array<string>();
        if (!m_Sections)
            return;
        for (int i = 0; i < m_Sections.Count(); i++)
            kinds.Insert(m_Sections[i].Kind);
    }

    bool OZ_IsWritten()
    {
        return m_Sections && m_Sections.Count() > 0;
    }

    int OZ_Used()
    {
        int total = 0;
        if (!m_Sections)
            return 0;
        for (int i = 0; i < m_Sections.Count(); i++)
            total += m_Sections[i].Records;
        return total;
    }

    // Стеля цього класу носія. Нуль у таблиці означає «без стелі», і назовні
    // це -1: нуль вільних місць і відсутність стелі -- протилежні речі, і
    // плутати їх не можна.
    int OZ_Max()
    {
        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(GetType());
        if (!spec || spec.MaxRecords <= 0)
            return -1;
        return spec.MaxRecords;
    }

    int OZ_Free()
    {
        int max = OZ_Max();
        if (max < 0)
            return -1;

        int free = max - OZ_Used();
        if (free < 0)
            free = 0;
        return free;
    }

    // Скільки влізе В ЦЕЙ РІД, якщо його переписати: місце, яке зараз займає
    // він сам, звільниться. Саме це число потрібне тому, хто збирається
    // обрізати свій список під носій.
    int OZ_RoomFor(string kind)
    {
        int max = OZ_Max();
        if (max < 0)
            return -1;

        int mine = OZ_Records(kind);
        if (mine < 0)
            mine = 0;

        int room = max - OZ_Used() + mine;
        if (room < 0)
            room = 0;
        return room;
    }

    // ------------------------------------------------------------- запис
    //
    // Серверний: вміст носія клієнт задавати не може. Порожній json стирає
    // саме цей рід, решта живе далі.
    //
    // Відмовляє, коли не влазить, а не обрізає: обрізати може лише той, хто
    // знає, що в JSON, -- він і мусить спитати OZ_RoomFor заздалегідь. Але
    // перевірка стоїть ТУТ, а не тільки в нього: писар, який забув спитати,
    // отримує відмову, а не тихе переповнення.
    bool OZ_Write(string kind, string json, int records)
    {
        if (!GetGame().IsServer())
            return false;
        if (kind == "")
            return false;

        if (json == "")
        {
            OZ_Drop(kind);
            return true;
        }

        if (records < 0)
            records = 0;

        int room = OZ_RoomFor(kind);
        if (room >= 0 && records > room)
        {
            string full = "carrier " + GetType() + ": " + kind;
            full += " needs " + records.ToString() + " records, room for " + room.ToString();
            OZ_Log.Warn(full);
            return false;
        }

        OZ_CarrierSection s = Find(kind);
        if (!s)
        {
            s = new OZ_CarrierSection();
            s.Kind = kind;
            m_Sections.Insert(s);
        }

        s.Payload = json;
        s.Records = records;
        return true;
    }

    void OZ_Drop(string kind)
    {
        if (!GetGame().IsServer() || !m_Sections)
            return;

        for (int i = m_Sections.Count() - 1; i >= 0; i--)
        {
            if (m_Sections[i].Kind == kind)
                m_Sections.Remove(i);
        }
    }

    void OZ_Erase()
    {
        if (!GetGame().IsServer())
            return;
        m_Sections.Clear();
    }

    // ------------------------------------------------- знайомі роди КПК
    //
    // Тонкі обгортки, і тільки. Решта КПК писалась під них, і переписувати її
    // заради нової форми немає підстав: рід тут -- звичайний рядок, а ці три
    // просто знає сам КПК.

    // РІД НЕСЕ ВЛАСНИКА, як і імена файлів у профілі. Простір родів спільний
    // для всіх модів, що пишуть на носії, і голе "markers" -- та сама міна, що
    // й голе "Radio.json": другий охочий колись з'явиться, і зіткнення буде
    // тихим. Домовленість проста -- oz_<мод>_<що>.
    static const string KIND_MARKS = "oz_pda_markers";
    static const string KIND_NOTES = "oz_pda_notes";
    static const string KIND_ROUTE = "oz_pda_route";

    string OZ_Marks()      { return OZ_Read(KIND_MARKS); }
    string OZ_Notes()      { return OZ_Read(KIND_NOTES); }
    string OZ_Route()      { return OZ_Read(KIND_ROUTE); }

    int OZ_MarkCount()     { return OZ_Records(KIND_MARKS); }
    int OZ_NoteCount()     { return OZ_Records(KIND_NOTES); }
    int OZ_RoutePts()      { return OZ_Records(KIND_ROUTE); }

    bool OZ_WriteMarks(string json, int count) { return OZ_Write(KIND_MARKS, json, count); }
    bool OZ_WriteNotes(string json, int count) { return OZ_Write(KIND_NOTES, json, count); }
    bool OZ_WriteRoute(string json, int count) { return OZ_Write(KIND_ROUTE, json, count); }

    // ---------------------------------------------------------- сховище
    //
    // ДОПИСУВАТИ тільки в кінець і читати за GetVersion(). Записи CF
    // позиційні: вставка поля в середину зсуває потік і з'їдає дані всіх, хто
    // пише після нас.
    //
    // Формат один -- "kv1", і старих він не читає. Мод живе на стенді
    // розробки, збережених носіїв ніде немає, а код, що вміє читати формати,
    // яких більше не існує, -- це код, який ніхто ніколи не перевірить.
    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return;

        ctx.Write("kv1");
        ctx.Write(m_Sections.Count());

        for (int i = 0; i < m_Sections.Count(); i++)
        {
            OZ_CarrierSection s = m_Sections[i];
            ctx.Write(s.Kind);
            ctx.Write(s.Records);
            // Секції -- шматками: стеля рядка сховища 1023 байти, подробиці
            // в OZ_StoreBig.
            OZ_StoreBig.Write(ctx, s.Payload);
        }
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage))
            return false;

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return true;

        string tag;
        if (!ctx.Read(tag))
            return false;

        if (tag != "kv1")
        {
            // Чужий або старий формат: читати його нема чим, і вдавати, що
            // прочитали, гірше за порожній носій.
            OZ_Log.Warn("carrier " + GetType() + ": unknown storage format " + tag + " - the carrier comes up empty");
            return true;
        }

        int count;
        if (!ctx.Read(count))
            return false;

        m_Sections.Clear();
        for (int i = 0; i < count; i++)
        {
            OZ_CarrierSection s = new OZ_CarrierSection();
            if (!ctx.Read(s.Kind))
                return false;
            if (!ctx.Read(s.Records))
                return false;
            if (!OZ_StoreBig.Read(ctx, s.Payload))
                return false;

            // ЧУЖИЙ РІД ЗБЕРІГАЄТЬСЯ. Раніше він тут губився -- і саме це
            // робило носій нерозширюваним: мод міг записати своє й побачити,
            // що після перезаходу цього немає.
            m_Sections.Insert(s);
        }

        return true;
    }
}


// Спільні ворота запису на носій. Одна логіка на ТРИ входи: гуртовий запис
// з пристрою, експорт мітки з карти, експорт записки з нотатника. Право
// писати не залежить від того, звідки прийшов запит.
class OZ_CarrierOps
{
    // ЧИТАННЯ: чип у гнізді пристрою в руках. Замок класу тут ні до чого
    // -- читається й замкнений на запис носій.
    static OZ_DataCarrier_Base Resolve(PlayerIdentity sender, out string error)
    {
        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return null;
        }

        OZ_DataCarrier_Base c = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
        if (!c)
        {
            error = "STR_OZ_ERR_NO_CARRIER";
            return null;
        }

        return c;
    }

    static OZ_DataCarrier_Base ResolveWritable(PlayerIdentity sender, out string error)
    {
        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return null;
        }

        OZ_DataCarrier_Base c = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
        if (!c)
        {
            error = "STR_OZ_ERR_NO_CARRIER";
            return null;
        }

        // Клас без запису в таблиці -- замок, як і клас із Writable=false.
        // Гейта роду тут БІЛЬШЕ НЕМАЄ: секції незалежні, і писати одну
        // поверх другої неможливо за побудовою.
        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (!spec || !spec.Writable)
        {
            error = "STR_OZ_ERR_CARRIER_LOCKED";
            return null;
        }

        return c;
    }
}


// Що віддає читання чипа: обидві секції ТИПІЗОВАНО плюс стелі класу.
// Не рядками: рядок-значення в JSON рушій ріже на 1023 байтах при розборі
// (зміряно зондом), і секція з однією повною запискою вже не влазила б.
// Вкладені ОБ'ЄКТИ серіалізуються полями, і кожне поле-значення всередині
// коротше за стелю за побудовою.
class OZ_CarrierView
{
    ref OZ_MarkerList Marks;
    ref OZ_NoteBook   Notes;
    // Спільна місткість і скільки з неї зайнято. Двох чисел досить, щоб
    // намалювати смужку заповнення для будь-якого набору родів -- зокрема
    // тих, про які ця сторінка не знає.
    int MaxRecords  = 0;
    int UsedRecords = 0;
}



