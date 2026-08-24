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
    // Вид вмісту. Збігається з id сторінки, яка вміє його читати.
    private string m_Kind    = "";
    // Сам вміст, як рядок JSON. Ані КПК, ані ядро в нього не заглядають.
    private string m_Payload = "";
    // Чи записаний носій узагалі. Порожній чип -- теж законний предмет.
    private bool   m_Written = false;

    string OZ_Kind()
    {
        return m_Kind;
    }

    string OZ_Payload()
    {
        return m_Payload;
    }

    bool OZ_IsWritten()
    {
        return m_Written;
    }

    // Серверна: вміст носія клієнт задавати не може.
    void OZ_Write(string kind, string payload)
    {
        if (!GetGame().IsServer())
            return;

        m_Kind    = kind;
        m_Payload = payload;
        m_Written = true;
    }

    void OZ_Erase()
    {
        if (!GetGame().IsServer())
            return;

        m_Kind    = "";
        m_Payload = "";
        m_Written = false;
    }

    // ДОПИСУВАТИ тільки в кінець і читати за GetVersion(). Записи CF
    // позиційні: вставка поля в середину зсуває потік і з'їдає дані всіх,
    // хто пише після нас.
    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        auto ctx = storage["OpenZone_PDA"];
        if (!ctx)
            return;

        ctx.Write(m_Written);
        ctx.Write(m_Kind);
        ctx.Write(m_Payload);
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
        if (!ctx.Read(m_Kind))
            return false;
        if (!ctx.Read(m_Payload))
            return false;

        return true;
    }
}
