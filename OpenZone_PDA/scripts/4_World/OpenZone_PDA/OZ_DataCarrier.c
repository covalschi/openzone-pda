// Носій даних: флешка, чип, папка з логами -- що завгодно, що вставляється
// в КПК і щось у собі несе.
//
// Носій НЕ знає, що на ньому записано. Він везе ВИД і корисне навантаження
// рядком JSON, а розбирає й малює це та сторінка, якій цей вид належить:
// "markers" -- сторінка міток, "chatlog" -- сторінка чату. Той самий принцип,
// що й у мосту: ядро возить конверти, а не читає листи.
//
// Наслідок, заради якого це так і зроблено: щоб додати новий тип носія, не
// треба чіпати ані КПК, ані ядро. Досить мода зі своєю сторінкою й своїм
// класнеймом у таблиці.

class OZ_DataCarrier_Base : ItemBase
{
    // ДВІ СЕКЦІЇ замість одного роду (рішення власника 2026-08-28): мітки й
    // нотатки живуть на одному чипі одночасно, і гейт роду помер разом із
    // m_Kind. Кожна секція -- свій JSON, порожній рядок -- секції немає.
    private string m_Marks = "";
    private string m_Notes = "";
    private string m_Route = "";
    // Чи записаний носій узагалі. Порожній чип -- теж законний предмет.
    private bool   m_Written = false;
    // Лічильники транзієнтні: рахуються з пейлоада при записі й при
    // завантаженні, тому формат сховища не росте. -1 -- секції немає.
    private int    m_MarkCount = -1;
    private int    m_NoteCount = -1;
    private int    m_RoutePts  = -1;

    string OZ_Marks()
    {
        return m_Marks;
    }

    string OZ_Notes()
    {
        return m_Notes;
    }

    string OZ_Route()
    {
        return m_Route;
    }

    int OZ_RoutePts()
    {
        return m_RoutePts;
    }

    bool OZ_IsWritten()
    {
        return m_Written;
    }

    int OZ_MarkCount()
    {
        return m_MarkCount;
    }

    int OZ_NoteCount()
    {
        return m_NoteCount;
    }

    // Серверні: вміст носія клієнт задавати не може. Порожній json стирає
    // саме цю секцію, друга живе далі.
    void OZ_WriteMarks(string json, int count)
    {
        if (!GetGame().IsServer())
            return;

        m_Marks = json;
        m_MarkCount = count;
        if (json == "")
            m_MarkCount = -1;

        RefreshWritten();
    }

    void OZ_WriteNotes(string json, int count)
    {
        if (!GetGame().IsServer())
            return;

        m_Notes = json;
        m_NoteCount = count;
        if (json == "")
            m_NoteCount = -1;

        RefreshWritten();
    }

    void OZ_WriteRoute(string json, int count)
    {
        if (!GetGame().IsServer())
            return;

        m_Route = json;
        m_RoutePts = count;
        if (json == "")
            m_RoutePts = -1;

        RefreshWritten();
    }

    void OZ_Erase()
    {
        if (!GetGame().IsServer())
            return;

        m_Marks = "";
        m_Notes = "";
        m_Route = "";
        m_MarkCount = -1;
        m_NoteCount = -1;
        m_RoutePts  = -1;
        m_Written = false;
    }

    private void RefreshWritten()
    {
        m_Written = (m_Marks != "" || m_Notes != "" || m_Route != "");
    }

    private static int CountMarks(string payload)
    {
        string err;
        OZ_MarkerList ml;
        if (JsonFileLoader<OZ_MarkerList>.LoadData(payload, ml, err) && ml && ml.Items)
            return ml.Items.Count();
        return -1;
    }

    private static int CountNotes(string payload)
    {
        string err;
        OZ_NoteBook nb;
        if (JsonFileLoader<OZ_NoteBook>.LoadData(payload, nb, err) && nb && nb.Notes)
            return nb.Notes.Count();
        return -1;
    }

    // ДОПИСУВАТИ тільки в кінець і читати за GetVersion(). Записи CF
    // позиційні: вставка поля в середину зсуває потік і з'їдає дані всіх,
    // хто пише після нас. Рядок-маркер "mix" на місці старого m_Kind -- це
    // водночас версія формату і сумісність: старі чипи несуть там свій рід.
    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return;

        ctx.Write(m_Written);
        // "mix2" -- формат із маршрутом. Читач розуміє й старіші.
        ctx.Write("mix2");
        // Секції -- шматками: стеля рядка сховища 1023 байти, подробиці
        // в OZ_StoreBig.
        OZ_StoreBig.Write(ctx, m_Marks);
        OZ_StoreBig.Write(ctx, m_Notes);
        OZ_StoreBig.Write(ctx, m_Route);
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage))
            return false;

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return true;

        if (!ctx.Read(m_Written))
            return false;

        string kind;
        if (!ctx.Read(kind))
            return false;

        if (kind == "mix2")
        {
            if (!OZ_StoreBig.Read(ctx, m_Marks))
                return false;
            if (!OZ_StoreBig.Read(ctx, m_Notes))
                return false;
            if (!OZ_StoreBig.Read(ctx, m_Route))
                return false;
        }
        else if (kind == "mix")
        {
            if (!OZ_StoreBig.Read(ctx, m_Marks))
                return false;
            if (!OZ_StoreBig.Read(ctx, m_Notes))
                return false;
        }
        else
        {
            // Старий одно-родовий чип: одна секція за родом, друга порожня.
            string payload;
            if (!OZ_StoreBig.Read(ctx, payload))
                return false;

            if (kind == "markers")
                m_Marks = payload;
            else if (kind == "notes")
                m_Notes = payload;
            // Чужий/зондовий рід свідомо губиться: читати його нема кому.
        }

        if (m_Marks != "")
            m_MarkCount = CountMarks(m_Marks);
        if (m_Notes != "")
            m_NoteCount = CountNotes(m_Notes);
        if (m_Route != "")
            m_RoutePts = CountMarks(m_Route);

        RefreshWritten();
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
    int MaxMarks = 0;
    int MaxNotes = 0;
}



