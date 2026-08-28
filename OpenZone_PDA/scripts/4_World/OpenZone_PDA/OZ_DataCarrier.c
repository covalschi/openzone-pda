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
    // Чи записаний носій узагалі. Порожній чип -- теж законний предмет.
    private bool   m_Written = false;
    // Лічильники транзієнтні: рахуються з пейлоада при записі й при
    // завантаженні, тому формат сховища не росте. -1 -- секції немає.
    private int    m_MarkCount = -1;
    private int    m_NoteCount = -1;

    string OZ_Marks()
    {
        return m_Marks;
    }

    string OZ_Notes()
    {
        return m_Notes;
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

    void OZ_Erase()
    {
        if (!GetGame().IsServer())
            return;

        m_Marks = "";
        m_Notes = "";
        m_MarkCount = -1;
        m_NoteCount = -1;
        m_Written = false;
    }

    private void RefreshWritten()
    {
        m_Written = (m_Marks != "" || m_Notes != "");
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
        ctx.Write("mix");
        // Секції -- шматками: стеля рядка сховища 1023 байти, подробиці
        // в OZ_StoreBig.
        OZ_StoreBig.Write(ctx, m_Marks);
        OZ_StoreBig.Write(ctx, m_Notes);
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

        if (kind == "mix")
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

        RefreshWritten();
        return true;
    }
}


// Спільні ворота запису на носій. Одна логіка на ТРИ входи: гуртовий запис
// з пристрою, експорт мітки з карти, експорт записки з нотатника. Право
// писати не залежить від того, звідки прийшов запит.
class OZ_CarrierOps
{
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

// «Злити нотатки З чипа»: спершу питаємо міст, СКІЛЬКИ МІСЦЯ лишилось у
// книжці одержувача, і шлемо рівно стільки. Без цього кроку імпорт у повну
// книжку рапортував би успіх, а міст мовчки відкидав би кожен лист: стеля
// живе в мості, і рахувати місце може лише той, хто спитав.
class OZ_CarrierImportReply : OZ_BridgeReply
{
    // Один імпорт на гравця за раз. Дві двофазки впереміш бачили б ОДНАКОВЕ
    // «зайнято» від моста, кожна порахувала б те саме місце і кожна слала б
    // свій комплект листів -- книжка гравця подвоїлась би. Латч знімає
    // БУДЬ-ЯКЕ завершення: OnBody, OnFail і тиша (OnQuiet іде в OnFail).
    protected static ref map<string, bool> s_Busy = new map<string, bool>();

    static bool Begin(string uid)
    {
        if (s_Busy.Contains(uid))
            return false;
        s_Busy.Set(uid, true);
        return true;
    }

    static void End(string uid)
    {
        if (s_Busy.Contains(uid))
            s_Busy.Remove(uid);
    }

    protected string m_Uid;
    // Знімок книжки З ЧИПА, як він лежав на предметі в мить запиту: чип за
    // час дороги міг зникнути з гнізда, а імпортуємо ми не гніздо, а вміст.
    protected string m_BookJson;
    // Підсумок міток, які вже лягли синхронно ПЕРЕД цим листом: фінальна
    // відповідь складає одну чесну цифру за ОБИДВІ секції. -1 -- міток не було.
    protected int m_MarksTaken;
    protected int m_MarksTotal;

    void OZ_CarrierImportReply(string uid, string bookJson, int marksTaken, int marksTotal)
    {
        m_Uid        = uid;
        m_BookJson   = bookJson;
        m_MarksTaken = marksTaken;
        m_MarksTotal = marksTotal;
    }

    override void OnBody(string json)
    {
        End(m_Uid);

        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        string err;

        OZ_NoteBook have;
        if (!JsonFileLoader<OZ_NoteBook>.LoadData(json, have, err) || !have || !have.Notes)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_import", false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        OZ_NoteBook book;
        if (!JsonFileLoader<OZ_NoteBook>.LoadData(m_BookJson, book, err) || !book || !book.Notes)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_import", false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        // Стелю каже міст; старий міст без поля Max покриває наша власна.
        int ceiling = have.Max;
        if (ceiling <= 0)
            ceiling = OZ_PdaConst.NOTES_MAX;

        int room = ceiling - have.Notes.Count();
        if (room <= 0)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_import", false, "", "STR_OZ_ERR_NOTES_FULL");
            return;
        }

        int wanted = book.Notes.Count();
        int cap = wanted;
        if (cap > room)
            cap = room;

        // Кожна записка -- окремий лист з ПОРОЖНІМ Id: «створи таку саму»,
        // а не «редагуй чужу». Межі -- ті самі, що й у прямого збереження.
        int sent = 0;
        for (int k = 0; k < cap; k++)
        {
            OZ_NotesAskSave a = new OZ_NotesAskSave();
            a.Uid   = m_Uid;
            a.Id    = "";
            a.Title = OZ_Text.Clip(book.Notes[k].Title, OZ_PdaConst.NOTE_TITLE_MAX);
            a.Body  = OZ_Text.Clip(book.Notes[k].Body, OZ_PdaConst.NOTE_BODY_MAX);
            a.Name  = to.GetName();

            string letter;
            if (!JsonFileLoader<OZ_NotesAskSave>.MakeData(a, letter, err, false))
                continue;

            OZ_BridgeClient.Call("v1/notes/save", letter, new OZ_NotesReply(m_Uid, "carrier_note", false));
            sent++;
        }

        OZ_Log.Info("carrier: importing " + sent.ToString() + "/" + wanted.ToString() + " note(s) for " + m_Uid + " (room " + room.ToString() + ")");

        OZ_CarrierTaken t = new OZ_CarrierTaken();
        t.Taken = sent;
        t.Total = wanted;
        if (m_MarksTotal > 0)
        {
            t.Taken += m_MarksTaken;
            t.Total += m_MarksTotal;
        }

        string tj;
        if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(t, tj, err, false))
            tj = "";

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_import", true, tj, "");
    }

    override void OnFail(int code)
    {
        End(m_Uid);

        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_import", false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}


// Відповідь на забір ОДНІЄЇ записки з чипа: міст або створив її в книжці,
// або чесно відмовив (повна книжка, тиша). Гравцеві їде рівно це.
class OZ_CarrierTakeReply : OZ_BridgeReply
{
    protected string m_Uid;

    void OZ_CarrierTakeReply(string uid)
    {
        m_Uid = uid;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_NotesFail fail;
        string err;
        if (JsonFileLoader<OZ_NotesFail>.LoadData(json, fail, err) && fail && fail.Error != "")
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_take", false, "", OZ_NotesFail.KeyOf(fail.Error));
            return;
        }

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_take", true, "", "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_take", false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}


// «Записати нотатки на чип»: книжка приїхала з моста -- кладемо на предмет.
// Пристрій і чип ПЕРЕВІРЯЄМО ЗАНОВО: за час дороги гравець міг викласти
// КПК чи висмикнути чип, і писати тоді нема куди.
class OZ_CarrierNotesReply : OZ_BridgeReply
{
    protected string m_Uid;
    // САМЕ ТОЙ чип, для якого запит дозволили. За час дороги гравець міг
    // підмінити його іншим -- зокрема замкненим чи чужим записаним.
    protected OZ_DataCarrier_Base m_Chip;

    void OZ_CarrierNotesReply(string uid, OZ_DataCarrier_Base chip)
    {
        m_Uid  = uid;
        m_Chip = chip;
    }

    override void OnBody(string json)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(to);
        if (!pda)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_NO_DEVICE");
            return;
        }

        OZ_DataCarrier_Base c = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
        if (!c)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_NO_CARRIER");
            return;
        }

        // Дозвіл давали ІНШОМУ чипові -- підміна під час дороги не успадковує
        // його. І замок класу перевіряємо заново з тієї ж причини.
        if (c != m_Chip)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_NO_CARRIER");
            return;
        }

        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (!spec || !spec.Writable)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_CARRIER_LOCKED");
            return;
        }

        // Тіло мусить БУТИ книжкою: будь-яку іншу двохсотку моста писати на
        // чип як «нотатки» означало б зіпсувати його чимось нечитним.
        OZ_NoteBook book;
        string berr;
        if (!JsonFileLoader<OZ_NoteBook>.LoadData(json, book, berr) || !book || !book.Notes)
        {
            OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_INTERNAL");
            return;
        }

        // Місткість класу: на малий носій лягають ПЕРШІ записки, і
        // відповідь чесно каже скільки з скількох.
        int total = book.Notes.Count();
        int wrote = total;
        if (spec.MaxNotes > 0 && total > spec.MaxNotes)
        {
            book.Notes.Resize(spec.MaxNotes);
            wrote = spec.MaxNotes;

            if (!JsonFileLoader<OZ_NoteBook>.MakeData(book, json, berr, false))
            {
                OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_INTERNAL");
                return;
            }
        }

        c.OZ_WriteNotes(json, wrote);

        OZ_CarrierTaken t = new OZ_CarrierTaken();
        t.Taken = wrote;
        t.Total = total;

        string tj;
        if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(t, tj, berr, false))
            tj = "";

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", true, tj, "");
    }

    override void OnFail(int code)
    {
        PlayerIdentity to = OZ_ChatWho.Online(m_Uid);
        if (!to)
            return;

        OZ_Rpc.Respond(to, OZ_PdaConst.PAGE_DEVICE, "carrier_write", false, "", "STR_OZ_ERR_NO_BRIDGE");
    }
}
