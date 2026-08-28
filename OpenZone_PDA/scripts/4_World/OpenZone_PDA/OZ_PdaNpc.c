// ПУБЛІЧНИЙ API ДЛЯ ЧУЖИХ МОДІВ: NPC у КПК.
//
// Квестовий мод (чи будь-який інший) НЕ чіпає ні сторінок, ні моста --
// він кличе ці три статики, і все інше робить конвеєр КПК:
//
//   OZ_PdaNpc.Register("sidorovich", "Сидорович");     // раз, на старті
//   OZ_PdaNpc.AddContact("sidorovich", uid);           // NPC у контактах
//   OZ_PdaNpc.Say("sidorovich", uid, "Заходь, діло є."); // репліка
//
// МЕХАНІКА. Розмова з NPC -- звичайний тред моста з родом "npc": правда
// живе в Discord, вебхук пише від імені NPC, гравець читає і відповідає
// тим самим чатом, що й людям. Відповіді гравця лягають у той самий тред
// -- квестовий мод читає їх звідти-ж (або чекає свого хука в наступній
// версії). Історію NPC-розмові клієнт НЕ довантажує: діалог скриптовий,
// його ведуть, а не гортають.
//
// Контакт NPC живе в OZ_PlayerData.NpcContacts псевдо-uid-ом "npc:<id>".
// Це СВІДОМО інший простір імен, ніж SteamID: NPC не може бути членом
// групи, другом чи ціллю обміну -- розділення, про яке просив власник,
// саме тут і забезпечується.

class OZ_PdaNpc
{
    // id -> показне ім'я. Реєстр рантаймовий: мод реєструє своїх NPC на
    // кожному старті, і це дешевше та чесніше за файл, який розсинхронʼується.
    private static ref map<string, string> s_Names = new map<string, string>();

    static void Register(string npcId, string name)
    {
        if (!GetGame().IsServer())
            return;
        if (npcId == "" || name == "")
            return;

        s_Names.Set(npcId, OZ_Text.Clip(name, 48));
        OZ_Log.Info("npc: registered " + npcId);
    }

    static string NameOf(string npcId)
    {
        string n;
        if (s_Names.Find(npcId, n))
            return n;
        return "";
    }

    static bool IsNpcUid(string uid)
    {
        return uid.IndexOf("npc:") == 0;
    }

    // Репліка NPC гравцеві. Створює розмову роду "npc" при першому слові;
    // далі рядок їде гравцеві звичайним пушем чату.
    static void Say(string npcId, string playerUid, string text)
    {
        if (!GetGame().IsServer())
            return;

        string name = NameOf(npcId);
        if (name == "")
        {
            OZ_Log.Warn("npc: Say for unregistered id " + npcId);
            return;
        }

        if (!OZ_BridgeClient.IsRunning())
        {
            OZ_Log.Warn("npc: bridge is down, Say(" + npcId + ") dropped");
            return;
        }

        OZ_NpcSayAsk a = new OZ_NpcSayAsk();
        a.NpcId = npcId;
        a.Name  = name;
        a.Uid   = playerUid;
        a.Text  = OZ_Text.Clip(text, OZ_PdaConst.CHAT_MSG_MAX);

        string letter;
        string err;
        if (!JsonFileLoader<OZ_NpcSayAsk>.MakeData(a, letter, err, false))
        {
            OZ_Log.Error("npc: cannot build the letter: " + err);
            return;
        }

        OZ_BridgeClient.Call("v1/npc/say", letter, new OZ_NpcSayReply(npcId));
    }

    // NPC у контактах гравця. Ідемпотентно.
    static void AddContact(string npcId, string playerUid)
    {
        if (!GetGame().IsServer())
            return;
        if (NameOf(npcId) == "")
        {
            OZ_Log.Warn("npc: AddContact for unregistered id " + npcId);
            return;
        }

        OZ_PlayerData d = OZ_PlayerStore.Load(playerUid);
        string tag = "npc:" + npcId;
        if (d.NpcContacts.Find(tag) == -1)
        {
            d.NpcContacts.Insert(tag);
            OZ_PlayerStore.Flush(playerUid);
        }
    }

    static void DropContact(string npcId, string playerUid)
    {
        if (!GetGame().IsServer())
            return;

        OZ_PlayerData d = OZ_PlayerStore.Load(playerUid);
        int at = d.NpcContacts.Find("npc:" + npcId);
        if (at >= 0)
        {
            d.NpcContacts.Remove(at);
            OZ_PlayerStore.Flush(playerUid);
        }
    }
}

// ------------------------------------------------------- листи до моста

class OZ_NpcSayAsk
{
    string NpcId;
    string Name;
    string Uid;
    string Text;
}

// Відповідь моста нікуди не їде -- гравець отримає САМ РЯДОК звичайним
// пушем чату. Тут лише слід у лозі, коли міст відмовив.
class OZ_NpcSayReply : OZ_BridgeReply
{
    protected string m_NpcId;

    void OZ_NpcSayReply(string npcId)
    {
        m_NpcId = npcId;
    }

    override void OnFail(int code)
    {
        OZ_Log.Warn("npc: Say(" + m_NpcId + ") failed at the bridge, code " + code.ToString());
    }
}
