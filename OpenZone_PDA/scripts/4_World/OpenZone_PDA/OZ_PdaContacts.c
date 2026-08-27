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
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "list")
            return List(sender, ok, error);

        if (op == "hide")
            return Hide(json, sender, ok, error);

        if (op == "friend_ask")
            return FriendAsk(json, sender, ok, error);

        if (op == "friend_accept")
            return FriendAnswer(json, sender, true, ok, error);

        if (op == "friend_decline")
            return FriendAnswer(json, sender, false, ok, error);

        if (op == "friend_drop")
            return FriendDrop(json, sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------- список

    private string List(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        string myUid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);
        PlayerBase mePlayer = OZ_PdaLookup.PlayerOf(sender);

        OZ_ContactList list = new OZ_ContactList();
        list.MeHidden = me.PresenceHidden;

        // Ким я є сам -- вирішує СЕРВЕР. Клієнт про свої права не здогадується
        // і намалює лідерські кнопки рівно тоді, коли йому тут скажуть.
        string myFaction = OZ_Factions.OfUid(myUid);
        list.MeLeader    = myFaction != "" && OZ_Roles.IsLeader(myUid);

        // Міст давно мовчить -- скажемо про це, а не покажемо порожнє. Гравець
        // мусить розуміти, що бачить останнє відоме, а не «нікого немає».
        list.Stale = OZ_Identity.Stale();

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
            bool isFriend = Has(me.Friends, uid);

            if (!isMe && !isFriend)
            {
                OZ_PlayerData d = OZ_PlayerStore.Load(uid);
                if (d.PresenceHidden)
                    continue;
            }

            string rel  = RelOf(me, uid, isFriend);
            bool   near = !isMe && WithinReach(mePlayer, players[i]);

            // КОГО ВЗАГАЛІ ВИДНО. Раніше -- усіх, хто в Зоні, і це було
            // рівнозначно списку гравців сервера в кожного в кишені.
            //
            // Тепер видно того, з ким тебе ЩОСЬ ПОВ'ЯЗУЄ:
            //   -- себе;
            //   -- контакт;
            //   -- незавершений обмін (він попросив або я попросив) -- інакше
            //      відповісти не було б кому;
            //   -- того, хто ПРОСТО ЗАРАЗ поруч, бо ти його бачиш очима, і
            //      без цього познайомитись не було б де.
            //
            // Решта Зони -- ні. Незнайомець за кілометр у списку не з'явиться.
            if (!isMe && !isFriend && rel == "" && !near)
                continue;

            OZ_ContactEntry e = new OZ_ContactEntry();
            e.Name = id.GetName();
            e.Me   = isMe;
            e.Rel  = rel;
            e.Near = near;

            // РОЛІ -- ЛИШЕ СВОЇМ. Хто він у Зоні -- фракція, звання, посади,
            // мітки -- видно тільки собі й контактам.
            //
            // Це не приховування заради приховування: у Зоні незнайомець і є
            // незнайомець, і дізнатись, що перед тобою борговець, можна лише
            // обмінявшись КПК. Список гравців з фракціями всіх присутніх
            // перетворював би розвідку на читання екрана.
            if (isMe || isFriend)
            {
                e.Faction = OZ_Factions.NameOf(OZ_Factions.Of(PlayerBase.Cast(players[i]), uid));
                Identify(e, uid, myFaction);
            }

            list.Entries.Insert(e);
            seen.Insert(uid);
        }

        // 2. Друзі та ті, хто просився, але зараз офлайн. Пропустити їх
        //    означало б втратити вхідний запит: попросили й вийшли -- і
        //    відповісти нема кому.
        AddOffline(list, me.Friends, seen, "friend", myFaction);
        AddOffline(list, me.FriendReq, seen, "got", myFaction);

        return Serialise(list, ok, error);
    }

    private void AddOffline(OZ_ContactList list, array<string> uids, array<string> seen, string rel, string myFaction)
    {
        for (int i = 0; uids && i < uids.Count(); i++)
        {
            string uid = uids[i];
            if (seen.Find(uid) != -1)
                continue;

            OZ_PlayerData d = OZ_PlayerStore.Load(uid);

            OZ_ContactEntry e = new OZ_ContactEntry();
            // Ім'я з кешу: гравця немає на сервері, спитати нема в кого.
            // Порожнє означає «жодного разу не входив після оновлення» --
            // тоді краще чесний прочерк, ніж Steam64 на екрані.
            e.Name = d.Name;
            if (e.Name == "")
                e.Name = "---";
            e.Rel  = rel;
            e.Near = false;

            // Те саме правило, що й для присутніх: ролі -- лише контактам.
            // Той, хто лише ПОПРОСИВСЯ, ще не контакт.
            if (rel == "friend")
            {
                // Гравця немає на сервері -- постачальника питати нема про
                // кого, лишається останнє відоме з його файлу.
                e.Faction = OZ_Factions.NameOf(OZ_Factions.Of(null, uid));
                Identify(e, uid, myFaction);
            }

            list.Entries.Insert(e);
            seen.Insert(uid);
        }
    }

    // Три осі, яких у списку не було: звання, посади, мітки.
    //
    // Уже людськими назвами -- слаг на екрані читається як помилка, а
    // перекладати його мусив би кожен, хто малює.
    private void Identify(OZ_ContactEntry e, string uid, string myFaction)
    {
        e.Rank = OZ_Identity.RankName(uid);

        OZ_Identity.PostNames(uid, e.Posts);
        OZ_Identity.TraitNames(uid, e.Traits);

        // Порівнюємо СЛАГИ, а не назви: дві однакові назви -- право адміна, і
        // це не привід зарахувати чужого в свої.
        e.Mine = myFaction != "" && OZ_Factions.OfUid(uid) == myFaction;
    }

    private string RelOf(OZ_PlayerData me, string uid, bool isFriend)
    {
        if (isFriend)
            return "friend";
        if (Has(me.FriendReq, uid))
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
        return Has(them.FriendReq, myUid);
    }

    private bool WithinReach(PlayerBase me, Man other)
    {
        if (!me || !other)
            return false;
        return vector.Distance(me.GetPosition(), other.GetPosition()) <= OZ_PdaConst.FRIEND_REACH_M;
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
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    // ----------------------------------------------------------- невидимка

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

    // -------------------------------------------------------------- друзі

    // Попросити в друзі того, хто стоїть поруч.
    private string FriendAsk(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        PlayerBase mePlayer = OZ_PdaLookup.PlayerOf(sender);
        string myUid = sender.GetPlainId();

        // Шукаємо серед ТИХ, ХТО ПОРУЧ, а не серед усіх: ім'я з клієнта --
        // це прохання, а не адреса, і сервер сам вирішує, кому воно
        // належить. Далекого однофамільця так не зачепиш.
        PlayerIdentity target = NearbyByName(mePlayer, r.Name, myUid);
        if (!target)
        {
            error = "STR_OZ_ERR_NOT_NEAR";
            return "";
        }

        string theirUid = target.GetPlainId();

        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);
        if (Has(me.Friends, theirUid))
        {
            error = "STR_OZ_ERR_ALREADY_FRIEND";
            return "";
        }

        OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);

        // Він уже просився до мене -- тоді це не новий запит, а згода.
        // Змушувати двох людей, які обидва натиснули «додати», ще й шукати
        // кнопку «прийняти», було б безглуздо.
        if (Has(me.FriendReq, theirUid))
            return FriendAnswerUid(myUid, theirUid, true, ok, error);

        if (Has(them.FriendReq, myUid))
        {
            error = "STR_OZ_ERR_ALREADY_ASKED";
            return "";
        }

        them.FriendReq.Insert(myUid);
        OZ_PlayerStore.MarkDirty(theirUid);

        ok = true;
        error = "";
        return "";
    }

    private string FriendAnswer(string json, PlayerIdentity sender, bool accept, out bool ok, out string error)
    {
        ok = false;

        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string myUid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        string theirUid = UidByNameIn(me.FriendReq, r.Name);
        if (theirUid == "")
        {
            error = "STR_OZ_ERR_NO_REQUEST";
            return "";
        }

        // ПРИЙНЯТИ -- ЛИШЕ ВІЧ-НА-ВІЧ, так само як і попросити.
        //
        // Обмін контактами -- це дія в світі, а не в меню: двоє стоять поруч і
        // тикають приладами. Дозволити приймати здалеку означало б, що
        // половина обміну відбувається в світі, а половина -- ні, і тоді
        // перша половина взагалі ні на що не впливає.
        //
        // ВІДМОВИТИ можна звідки завгодно: сказати «ні» -- не зустріч, і
        // тримати людину заручником чужого запиту, поки вона його не
        // наздожене, було б знущанням.
        if (accept)
        {
            PlayerBase mePlayer = OZ_PdaLookup.PlayerOf(sender);
            if (!NearbyUid(mePlayer, theirUid, myUid))
            {
                error = "STR_OZ_ERR_NOT_NEAR";
                return "";
            }
        }

        return FriendAnswerUid(myUid, theirUid, accept, ok, error);
    }

    private string FriendAnswerUid(string myUid, string theirUid, bool accept, out bool ok, out string error)
    {
        ok = false;

        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        int at = me.FriendReq.Find(theirUid);
        if (at != -1)
            me.FriendReq.Remove(at);

        if (accept)
        {
            // Записуємо ОБОМ. Дружба взаємна, і однобокий запис зробив би її
            // видимою лише з одного боку -- тобто зламаною там, де це
            // найважче помітити.
            if (!Has(me.Friends, theirUid))
                me.Friends.Insert(theirUid);

            OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);
            if (!Has(them.Friends, myUid))
                them.Friends.Insert(myUid);

            OZ_PlayerStore.MarkDirty(theirUid);
        }

        OZ_PlayerStore.MarkDirty(myUid);

        ok = true;
        error = "";
        return "";
    }

    private string FriendDrop(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_NameRef r;
        string err;
        if (!JsonFileLoader<OZ_NameRef>.LoadData(json, r, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string myUid = sender.GetPlainId();
        OZ_PlayerData me = OZ_PlayerStore.Load(myUid);

        string theirUid = UidByNameIn(me.Friends, r.Name);
        if (theirUid == "")
        {
            error = "STR_OZ_ERR_NO_FRIEND";
            return "";
        }

        int at = me.Friends.Find(theirUid);
        if (at != -1)
            me.Friends.Remove(at);

        // І в нього теж: дружба взаємна в обидва боки, зокрема й коли її
        // розривають. Лишити запис у нього означало б, що він і далі бачить
        // того, хто його з друзів викреслив.
        OZ_PlayerData them = OZ_PlayerStore.Load(theirUid);
        int at2 = them.Friends.Find(myUid);
        if (at2 != -1)
            them.Friends.Remove(at2);

        OZ_PlayerStore.MarkDirty(myUid);
        OZ_PlayerStore.MarkDirty(theirUid);

        ok = true;
        error = "";
        return "";
    }

    // Чи цей конкретний гравець зараз у межах простягнутої руки.
    //
    // За uid, а не за іменем: тут ми вже знаємо, кого шукаємо, і двоє
    // однакових імен не мусять вирішувати, кого приймають у друзі.
    private bool NearbyUid(PlayerBase me, string uid, string myUid)
    {
        if (!me || uid == "" || uid == myUid)
            return false;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;

            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;
            if (id.GetPlainId() != uid)
                continue;

            return WithinReach(me, players[i]);
        }
        return false;
    }

    private PlayerIdentity NearbyByName(PlayerBase me, string name, string myUid)
    {
        if (!me || name == "")
            return null;

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            PlayerIdentity id = players[i].GetIdentity();
            if (!id || id.GetPlainId() == myUid)
                continue;
            if (id.GetName() != name)
                continue;
            if (!WithinReach(me, players[i]))
                continue;
            return id;
        }
        return null;
    }

    private string UidByNameIn(array<string> uids, string name)
    {
        for (int i = 0; uids && i < uids.Count(); i++)
        {
            OZ_PlayerData d = OZ_PlayerStore.Load(uids[i]);
            if (d.Name == name)
                return uids[i];
        }
        return "";
    }
}
