// Сторінка «Контакти»: хто зараз у Зоні.
//
// Список -- це ПРИСУТНІСТЬ, а не транспондер, і плутати їх не можна:
// присутність -- твоє ім'я на весь сервер, транспондер -- твоя точка на
// чужій карті в радіусі антени. Вимикачі незалежні (див. OZ_PlayerData).
//
// Хто сховався -- того в списку немає ЗОВСІМ, і лічильник його теж не
// рахує. Показати «онлайн: 7», перелічивши шістьох, означає сказати рівно
// те, що невидимка й ховає.

class OZ_PdaHandlerContacts : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "list")
            return List(sender, ok, error);

        if (op == "hide")
            return Hide(json, sender, ok, error);

        return "";
    }

    private string List(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        string myUid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        OZ_ContactList list = new OZ_ContactList();
        list.MeHidden = me.PresenceHidden;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;

            string uid = id.GetPlainId();
            bool isMe = (uid == myUid);

            // Себе видно завжди -- інакше, увімкнувши невидимку, гравець
            // побачив би порожній список і вирішив, що зламав пристрій.
            if (!isMe)
            {
                OZ_PlayerData d = OZ_PlayerStore.Load(uid);
                if (d.PresenceHidden)
                    continue;
            }

            OZ_ContactEntry e = new OZ_ContactEntry();
            e.Name = id.GetName();
            e.Me   = isMe;
            list.Entries.Insert(e);
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_ContactList>.MakeData(list, outJson, err, false))
        {
            OZ_Log.Error("contact list serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Hide(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PdaFlagOp flag;
        string err;
        if (!JsonFileLoader<OZ_PdaFlagOp>.LoadData(json, flag, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string uid = sender.GetPlainId();
        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        d.PresenceHidden = flag.Value;
        OZ_PlayerStore.MarkDirty(uid);

        ok = true;
        error = "";
        return "";
    }
}
