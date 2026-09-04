// Сторінка «Контакти»: хто зараз у Зоні і хто тобі свій.
//
// СПИСОК ОНЛАЙНУ -- це ПРИСУТНІСТЬ, а не транспондер, і плутати їх не можна:
// присутність -- твоє ім'я на весь сервер, транспондер -- твоя точка на чужій
// карті в радіусі антени. Вимикачі незалежні (див. OZ_PlayerData).
//
// Хто сховався -- того в списку немає ЗОВСІМ, і лічильника окремо теж немає:
// він дорівнює довжині списку, а будь-яке інше число підказало б рівно те,
// що невидимка й ховає. ДРУЗІ -- виняток: друга видно завжди, і в цьому
// половина сенсу дружби. Хто не хоче, щоб його бачив саме цей -- видаляє
// його з друзів, а не ховається від усіх.
//
// ДРУЖБА ВЗАЄМНА і потребує згоди. Попросити можна лише ЗБЛИЗЬКА, і це
// перевіряє сервер. Заразом це знімає питання «звідки клієнт узяв чужий
// Steam64»: нізвідки. По проводу їде лише ім'я, а сервер сам знаходить, кому
// воно належить, серед тих, хто справді поруч.

class OZ_PdaHandlerContacts : OZ_PageHandler
{
    // Акаунт називає ПРИСТРІЙ -- див. OZ_PdaHandlerChat. Заморозки тут не
    // буває: капсулу до контактів не пускають ворота, а їхній стан на мить
    // заморозки лежить у капсульному дайджесті сторінки пристрою.
    private string m_Acc;

    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        m_Acc = sender.GetPlainId();
        OZ_PDA_Base accDev = OZ_PdaLookup.HeldBy(sender);
        if (accDev && accDev.OZ_SessionUid() != "")
        {
            m_Acc = accDev.OZ_SessionUid();

            // КАПСУЛА: список -- імена з дайджеста, і більш нічого. Живої
            // присутності тут немає навмисне: заморожений прилад не бачить
            // світ, він пам'ятає людей. Будь-яка дія -- відмова.
            if (OZ_PdaCapsule.IsFrozen(accDev))
            {
                if (op != "list")
                {
                    error = "STR_OZ_ERR_FROZEN";
                    return "";
                }
                return FrozenList(accDev, ok, error);
            }
        }

        if (op == "list")
            return List(sender, ok, error);

        // Два вимикачі -- дві операції (ТЗ-4 R-A1.1).
        if (op == "hide_zone")
            return Hide(json, sender, true, ok, error);

        if (op == "hide_contacts")
            return Hide(json, sender, false, ok, error);

        // ask / accept / decline ЗНЯТІ. Обмін відбувається в світі -- дією з
        // приладом у руках, наведеною на людину (OZ_ActionExchangeContacts),
        // -- і сервер більше не приймає їх навіть від підробленого запиту:
        // операції, якої немає в інтерфейсі, не мусить бути й на межі.
        if (op == "friend_drop")
            return FriendDrop(json, sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------- список

    // Капсульні контакти: що пристрій встиг запам'ятати, поки був живим.
    private string FrozenList(OZ_PDA_Base pda, out bool ok, out string error)
    {
        OZ_ContactList list = new OZ_ContactList();
        list.Frozen = true;

        string serr;
        OZ_PdaSnapshot snap;
        if (pda.OZ_Snapshot() != "" && JsonFileLoader<OZ_PdaSnapshot>.LoadData(pda.OZ_Snapshot(), snap, serr) && snap && snap.Contacts)
        {
            for (int i = 0; i < snap.Contacts.Count(); i++)
            {
                OZ_ContactEntry e = new OZ_ContactEntry();
                e.Name = snap.Contacts[i];
                e.Rel  = "friend";
                list.Entries.Insert(e);
            }
        }

        string outJson;
        if (!JsonFileLoader<OZ_ContactList>.MakeData(list, outJson, serr, false))
        {
            error = "STR_OZ_ERR_PDA_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string List(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        string myUid = m_Acc;
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);
        PlayerBase mePlayer = OZ_PdaLookup.PlayerOf(sender);

        OZ_ContactList list = new OZ_ContactList();
        list.MeHiddenZone     = me.HiddenFromZone;
        list.MeHiddenContacts = me.HiddenFromContacts;

        // Ким я є сам -- вирішує СЕРВЕР. Клієнт про свої права не здогадується
        // і намалює лідерські кнопки рівно тоді, коли йому тут скажуть.
        // МОЄ УГРУПОВАННЯ. Саме воно вирішує і «я лідер», і «свій/чужий»:
        // у базової фракції немає ні лідера, ні складу (ТЗ-1 §5).
        string myFaction = OZ_Identity.Get().OrgOf(myUid);
        // Складену логіку -- у локальну змінну, у поле кладемо готове
        // значення. Причина -- вимір 2026-09-01: ланцюжок «&&», присвоєний
        // прямо в поле об'єкта на купі, псує купу (див. OZR_Page.Book).
        bool meLeader = false;
        if (myFaction != "")
            meLeader = OZ_Identity.Get().IsLeader(myUid);
        list.MeLeader = meLeader;

        // Міст давно мовчить -- скажемо про це, а не покажемо порожнє. Гравець
        // мусить розуміти, що бачить останнє відоме, а не «нікого немає».
        list.Stale = OZ_Identity.Get().Stale();

        // Чи хтось кличе мене у фракцію -- ЧЕРЕЗ СЛУЖБУ, а не через клас мода
        // фракцій. Раніше тут стояв прямий OZ_FactionInvites.Pending, і КПК
        // через нього не компілювався без того мода взагалі.
        string invFaction;
        string invFrom;
        if (OZ_Identity.Get().PendingInvite(myUid, invFaction, invFrom))
        {
            list.InviteFaction = OZ_Identity.Get().FactionName(invFaction);
            list.InviteFrom    = invFrom;
        }

        // 1. Ті, хто на сервері. Себе -- завжди, друга -- завжди, решту --
        //    якщо не сховались.
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        array<string> seen = new array<string>();

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;

            string uid = id.GetPlainId();
            bool isMe = (uid == myUid);

            // У записнику люди позначені КЛЮЧЕМ ПЕРСОНАЖА, а не Steam64:
            // той самий акаунт після пермадесу -- вже інша людина, і його
            // нове життя в чужих контактах не з'являється саме тому, що
            // ключ інший.
            string charKey = OZ_PlayerStore.KeyOf(uid);
            bool isFriend = Has(me.Friends, charKey);

            // Сховався -- значить сховався ВІД УСІХ, і від друзів теж
            // (рішення власника 2026-08-28: перша редакція лишала друзям
            // видимість, і в грі це читалось як поломка). Схований друг
            // не зникає з книжки -- він падає нижче, в ОФЛАЙН: людини в
            // Зоні не видно, і байдуже чому.
            if (!isMe)
            {
                OZ_PlayerData d = OZ_PlayerStore.Load(uid);

                // Від ЗАПИСНИКІВ -- геть зовсім: ключ у seen, і AddOffline
                // його теж пропустить. Від ЗОНИ -- лише вниз, у офлайн
                // (ТЗ-4 R-A1.1: два вимикачі, кожен про своє).
                if (d.HiddenFromContacts)
                {
                    seen.Insert(charKey);
                    continue;
                }
                if (d.HiddenFromZone)
                    continue;
            }

            // У СПИСКУ -- ТІЛЬКИ КОНТАКТИ. І ти сам.
            //
            // Ані чужих, ані тих, хто поруч, ані незавершених обмінів. Обмін
            // тепер відбувається В СВІТІ -- дією з приладом у руках, наведеною
            // на людину, -- тож у меню йому нема чого показувати: ні кого
            // додавати, ні кого приймати.
            //
            // Раніше тут був увесь сервер, і це робило КПК списком гравців у
            // кожного в кишені. Тепер це записник: у ньому ті, з ким ти справді
            // зустрічався.
            if (!isMe && !isFriend)
                continue;

            OZ_ContactEntry e = new OZ_ContactEntry();
            e.Name = id.GetName();
            e.Key  = OZ_Names.KeyOf(charKey);
            e.Me   = isMe;
            e.Rel  = "";
            if (isFriend)
                e.Rel = "friend";
            bool near = false;
            if (!isMe)
                near = WithinReach(mePlayer, players[i]);
            e.Near = near;

            // РОЛІ -- ЛИШЕ СВОЇМ. Хто він у Зоні -- фракція, звання, посади,
            // мітки -- видно тільки собі й контактам.
            //
            // Це не приховування заради приховування: у Зоні незнайомець і є
            // незнайомець, і дізнатись, що перед тобою борговець, можна лише
            // обмінявшись КПК. Список гравців з фракціями всіх присутніх
            // перетворював би розвідку на читання екрана.
            e.Online = true;

            if (isMe || isFriend)
            {
                string fid = OZ_Identity.Get().OrgOfPlayer(PlayerBase.Cast(players[i]), uid);
                e.Org      = OZ_Identity.Get().FactionName(fid);
                e.OrgColor = OZ_Identity.Get().FactionColor(fid, 255);
                e.Base     = OZ_Identity.Get().FactionName(OZ_Identity.Get().BaseOf(uid));
                Identify(e, uid, myFaction);
            }

            list.Entries.Insert(e);
            seen.Insert(charKey);
        }

        // 2. Друзі та ті, хто просився, але зараз офлайн. Пропустити їх
        //    означало б втратити вхідний запит: попросили й вийшли -- і
        //    відповісти нема кому.
        // Лише контакти. Вхідні пропозиції в списку більше не з'являються --
        // на них не відповідають з меню.
        AddOffline(list, me.Friends, seen, "friend", myFaction);

        // NPC-контакти -- окремим родом. Ім'я дає реєстр OZ_PdaNpc; NPC,
        // якого мод цього старту не зареєстрував, чесно не показується.
        for (int ni = 0; ni < me.NpcContacts.Count(); ni++)
        {
            string tag = me.NpcContacts[ni];
            if (tag.IndexOf("npc:") != 0)
                continue;

            string npcName = OZ_PdaNpc.NameOf(tag.Substring(4, tag.Length() - 4));
            if (npcName == "")
                continue;

            OZ_ContactEntry ne = new OZ_ContactEntry();
            ne.Name   = npcName;
            ne.Key    = tag;
            ne.Rel    = "npc";
            ne.Online = true;
            // Показний підзаголовок без нового клієнтського коду: рядок
            // звання рендериться під іменем, і "NPC" там читається як слід.
            ne.Rank   = "NPC";
            list.Entries.Insert(ne);
        }

        return Serialise(list, ok, error);
    }

    // Ті, кого зараз немає в Зоні. Сюди ж потрапляють ЗАМОРОЖЕНІ персонажі
    // -- ті, чиє життя скінчилось: у записнику вони лишаються назавжди й
    // виглядають рівно як будь-хто, хто давно не заходив. Різниці не видно
    // навмисно (рішення власника 2026-08-30): КПК не розповідає про смерть.
    private void AddOffline(OZ_ContactList list, array<string> keys, array<string> seen, string rel, string myFaction)
    {
        for (int i = 0; keys && i < keys.Count(); i++)
        {
            string key = keys[i];
            if (seen.Find(key) != -1)
                continue;

            string uid = OZ_PlayerStore.UidOfKey(key);

            // Живий запис або заморожений знімок -- ByKey знає, який саме.
            OZ_PlayerData d = OZ_PlayerStore.ByKey(key);

            // Схований від записників -- його немає й тут (R-A1.1).
            if (d && d.HiddenFromContacts)
                continue;

            OZ_ContactEntry e = new OZ_ContactEntry();
            // Ім'я з кешу: гравця немає на сервері, спитати нема в кого.
            // Порожнє означає «жодного разу не входив після оновлення» --
            // тоді краще чесний прочерк, ніж Steam64 на екрані.
            if (d)
                e.Name = d.Name;
            if (e.Name == "")
                e.Name = "#STR_OZ_CONTACT_NONAME";
            e.Key  = OZ_Names.KeyOf(key);
            e.Rel  = rel;
            e.Near = false;

            // КОЛИ ЙОГО БАЧИЛИ ОСТАННІЙ РАЗ. Для замороженого це мить його
            // останнього входу й назавжди вона: запис більше не оновиться.
            if (d)
                e.LastSeen = d.LastSeen;

            // Те саме правило, що й для присутніх: ролі -- лише контактам.
            // Той, хто лише ПОПРОСИВСЯ, ще не контакт.
            if (rel == "friend")
            {
                // Гравця немає на сервері -- постачальника питати нема про
                // кого, лишається останнє відоме з його файлу. У замороженого
                // це знімок його останнього дня.
                string ofid = "";
                string obase = "";
                if (d)
                {
                    ofid  = d.SeenOrg;
                    obase = d.SeenBase;
                }

                e.Org      = OZ_Identity.Get().FactionName(ofid);
                e.OrgColor = OZ_Identity.Get().FactionColor(ofid, 255);
                e.Base     = OZ_Identity.Get().FactionName(obase);

                if (OZ_PlayerStore.IsLive(key))
                    IdentifySeen(e, uid, myFaction, ofid);
                else
                    IdentifyFrozen(e, d, myFaction, ofid);
            }

            list.Entries.Insert(e);
            seen.Insert(key);
        }
    }

    // Знімок замороженого персонажа: звання й мітки беремо З ЙОГО ФАЙЛА, а
    // не з живого запису акаунта -- інакше в записнику під старим іменем
    // світилися б ролі нового життя, і це саме те, чого тут не має бути.
    private void IdentifyFrozen(OZ_ContactEntry e, OZ_PlayerData d, string myFaction, string ofid)
    {
        if (!d)
            return;

        // ЧЕРЕЗ СЛУЖБУ, а не через OZ_RoleNames мода фракцій: те саме джерело
        // (Seen* читає той самий файл гравця), але без імені, якого без того
        // мода не існує -- а через нього КПК не компілювався зовсім.
        e.Rank = OZ_Identity.Get().SeenRankName(d.SteamId);

        array<string> traits = new array<string>();
        OZ_Identity.Get().SeenTraitNames(d.SteamId, traits);
        for (int i = 0; i < traits.Count(); i++)
            e.Traits.Insert(traits[i]);

        bool mine = false;
        if (myFaction != "")
            mine = (ofid == myFaction);
        e.Mine = mine;
    }

    // Три осі, яких у списку не було: звання, посади, мітки.
    //
    // Уже людськими назвами -- слаг на екрані читається як помилка, а
    // перекладати його мусив би кожен, хто малює.
    private void Identify(OZ_ContactEntry e, string uid, string myFaction)
    {
        e.Rank = OZ_Identity.Get().RankName(uid);

        // ПОСАД НЕ ШЛЕМО. Хто у фракції лідер чи сержант -- справа тієї
        // фракції, і видно це в її половині вкладки (рішення власника
        // 2026-08-30). Не показуємо -- отже й не возимо: конверт, який
        // несе те, чого ніхто не малює, рано чи пізно десь намалюється.
        OZ_Identity.Get().TraitNames(uid, e.Traits);

        // Порівнюємо СЛАГИ, а не назви: дві однакові назви -- право адміна, і
        // це не привід зарахувати чужого в свої.
        bool mine = false;
        if (myFaction != "")
            mine = OZ_Identity.Get().OrgOf(uid) == myFaction;
        e.Mine = mine;
    }

    // Те саме для того, кого немає на сервері. Проекція ролей живе рівно
    // стільки, скільки гравець у мережі: щойно він вийшов, Forget прибирає
    // запис -- і офлайновий контакт ставав голим ім'ям. Свій сержант з Боргу
    // виглядав як випадковий перехожий, і лідерові пропонували «запросити»
    // того, хто вже рік у фракції.
    //
    // Беремо знімок останнього входу. Він може бути застарілим -- список і
    // так каже про це рядком угорі, коли міст мовчить.
    private void IdentifySeen(OZ_ContactEntry e, string uid, string myFaction, string ofid)
    {
        e.Rank = OZ_Identity.Get().SeenRankName(uid);
        OZ_Identity.Get().SeenTraitNames(uid, e.Traits);

        bool mine = false;
        if (myFaction != "")
            mine = (ofid == myFaction);
        e.Mine = mine;
    }

    private string RelOf(OZ_PlayerData me, string uid, bool isFriend)
    {
        if (isFriend)
            return "friend";
        // Вхідні пропозиції -- типізований список зі строком (OZ_FriendReq),
        // тому не Has, а пошук за ключем.
        if (OZ_PdaContactSwap.IndexOfReq(me.FriendReq, uid) != -1)
            return "got";
        if (SentTo(me.SteamId, uid))
            return "sent";
        return "";
    }

    // Чи я вже просився до нього. Питаємо ЙОГО файл: вхідні запити лежать у
    // того, кого просять, і другого списку «вихідних» ми не ведемо -- два
    // списки про одне й те саме розходяться першої ж миті.
    private bool SentTo(string myUid, string theirUid)
    {
        OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);
        return OZ_PdaContactSwap.IndexOfReq(them.FriendReq, myUid) != -1;
    }

    private bool WithinReach(PlayerBase me, Man other)
    {
        if (!me || !other)
            return false;
        return vector.Distance(me.GetPosition(), other.GetPosition()) <= OZ_PdaTune.FriendReachM();
    }

    private bool Has(array<string> a, string v)
    {
        if (!a)
            return false;
        return a.Find(v) != -1;
    }

    private string Serialise(OZ_ContactList list, out bool ok, out string error)
    {
        string outJson;
        string err;
        if (!JsonFileLoader<OZ_ContactList>.MakeData(list, outJson, err, false))
        {
            OZ_Log.Error("contact list serialise failed: " + err);
            error = "STR_OZ_ERR_PDA_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    // ----------------------------------------------------------- невидимка

    // zone=true -- вимикач «від Зони», інакше -- «від записників».
    private string Hide(string json, PlayerIdentity sender, bool zone, out bool ok, out string error)
    {
        ok = false;

        OZ_PdaFlagOp flag;
        string err;
        if (!JsonFileLoader<OZ_PdaFlagOp>.LoadData(json, flag, err))
        {
            error = "STR_OZ_ERR_PDA_INTERNAL";
            return "";
        }

        string uid = m_Acc;
        OZ_PlayerData d = OZ_PlayerStore.Load(uid);
        if (zone)
            d.HiddenFromZone = flag.Value;
        else
            d.HiddenFromContacts = flag.Value;
        OZ_PlayerStore.MarkDirty(uid);

        ok = true;
        error = "";
        return "";
    }

    // -------------------------------------------------------------- друзі

    private string FriendDrop(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_PDA_INTERNAL";
            return "";
        }

        string myUid = m_Acc;
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        // NPC-контакт (ТЗ-4 R-G2.3): кнопка «прибрати» на його рядку досі
        // гарантовано відмовляла -- у записнику друзів такого ключа немає.
        // NPC живуть у своєму просторі імен ("npc:<id>"), і прибирають їх
        // тим же публічним входом, яким їх додавали.
        if (r.Key.IndexOf("npc:") == 0)
        {
            OZ_PdaNpc.DropContact(r.Key.Substring(4, r.Key.Length() - 4), myUid);
            ok = true;
            error = "";
            return "";
        }

        string theirKey = UidByKeyIn(me.Friends, r.Key);
        if (theirKey == "")
        {
            error = "STR_OZ_ERR_NO_FRIEND";
            return "";
        }

        int at = me.Friends.Find(theirKey);
        if (at != -1)
            me.Friends.Remove(at);
        OZ_PlayerStore.MarkDirty(myUid);

        // ВЗАЄМНІСТЬ -- ЛИШЕ З ЖИВИМ. Дружбу розривають з обох боків, і поки
        // людина та сама, це правильно: лишити запис у нього означало б, що
        // він і далі бачить того, хто його викреслив.
        //
        // Але заморожений персонаж -- це вже не той, хто носить сьогодні цей
        // Steam64. Викреслити «у нього» означало б залізти в записник ЖИВОЇ
        // людини й прибрати звідти когось, кого вона й не чіпала.
        if (!OZ_PlayerStore.IsLive(theirKey))
        {
            ok = true;
            error = "";
            return "";
        }

        string theirUid = OZ_PlayerStore.UidOfKey(theirKey);
        string myKey    = OZ_PlayerStore.KeyOf(myUid);

        OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);
        int at2 = them.Friends.Find(myKey);
        if (at2 != -1)
            them.Friends.Remove(at2);

        OZ_PlayerStore.MarkDirty(theirUid);

        // Розірваний контакт заморожує особисту розмову: читати можна,
        // писати -- ні, доки руки не потиснуть знову (рішення власника
        // 2026-08-29).
        OZ_PairFreeze.Send("v1/chat/pair_freeze", myUid, theirUid);

        ok = true;
        error = "";
        return "";
    }

    // Шукаємо за КЛЮЧЕМ, а не за іменем: чому саме так -- у OZ_Names.
    private string UidByKeyIn(array<string> uids, string key)
    {
        return OZ_Names.PickIn(uids, key);
    }
}
