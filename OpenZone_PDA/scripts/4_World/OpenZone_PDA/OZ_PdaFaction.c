// Сторінка «Фракція»: хто ми, хто в нас, і фракційні дії. Все, що тут
// відбувається, йде через OZ_RoleOps (лідерські дії -- RoleRequest прямо з
// клієнта, як і раніше); сторінка лише ЗБИРАЄ стан в одну відповідь.
//
// Оновлюється САМА: ядро дзвонить у OZ_RoleNotify на кожну зміну проекції,
// а модуль розносить "push" усім онлайн -- і цій сторінці, і контактам.

class OZ_PdaHandlerFaction : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok    = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "state")
            return State(sender, ok, error);

        return "";
    }

    private string State(PlayerIdentity sender, out bool ok, out string error)
    {
        string uid = sender.GetPlainId();

        // Акаунт називає ПРИСТРІЙ -- та сама доктрина, що в розмов.
        OZ_PDA_Base dev = OZ_PdaLookup.HeldBy(sender);
        if (dev && dev.OZ_SessionUid() != "")
            uid = dev.OZ_SessionUid();

        OZ_FactionState st = new OZ_FactionState();

        // БАЗОВА фракцiя тут не рахується за фракцiю. «Сталкери» -- це всi
        // в Зонi, а не органiзацiя: складу в неї немає, лiдера немає, i
        // поiменний перелiк усiх сталкерiв сервера на цьому екранi був би
        // не лише безглуздий, а й видав би людей, яких нiхто не питав.
        // Екран каже чесне «фракцiї немає» -- те саме, що одинаковi.
        string slug = OZ_Factions.OrgOfUid(uid);
        st.Faction = slug;

        OZ_FactionInvite inv = OZ_FactionInvites.Pending(uid);
        if (inv)
        {
            st.InviteFaction = OZ_Factions.NameOf(inv.Faction);
            st.InviteFrom    = inv.FromName;
        }

        if (slug == "")
        {
            ok = true;
            error = "";
            return Serialise(st, ok, error);
        }

        st.FactionName = OZ_Factions.NameOf(slug);
        st.Color       = OZ_Factions.ColorARGB(slug);
        st.MyRank      = OZ_RoleNames.Of(OZ_Roles.RankOf(uid));
        st.MeLeader    = OZ_Roles.IsLeader(uid);

        // Драбина фракції -- щоб лідер міг підвищувати й знижувати, не
        // набираючи слагів: клієнт бере сусідню сходинку сам.
        OZ_Roles.FRankLadder(slug, st.RankIds, st.RankNames);

        // Члени: кеш проекцій за цей запуск плюс усі онлайн із цією
        // фракцією -- і я сам. Повного вічного списку сервер не має, і
        // чесніше показати відоме, ніж вигадувати.
        array<string> uids = new array<string>();
        OZ_Roles.FactionMembers(slug, uids);

        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);
        for (int pi = 0; pi < players.Count(); pi++)
        {
            if (!players[pi])
                continue;
            PlayerIdentity oid = players[pi].GetIdentity();
            if (!oid)
                continue;
            string ou = oid.GetPlainId();
            if (OZ_Factions.OfUid(ou) == slug && uids.Find(ou) == -1)
                uids.Insert(ou);
        }
        if (uids.Find(uid) == -1)
            uids.Insert(uid);

        for (int i = 0; i < uids.Count(); i++)
        {
            OZ_FactionMember m = new OZ_FactionMember();
            OZ_PlayerData md = OZ_PlayerStore.Load(uids[i]);
            m.Name   = md.Name;
            if (m.Name == "")
                continue;   // безіменний кеш нікому нічого не скаже
            m.Rank    = OZ_RoleNames.Of(OZ_Roles.RankOf(uids[i]));
            m.FRankId = OZ_Roles.FRankOf(uids[i]);
            if (m.FRankId != "")
                m.FRank = OZ_RoleNames.Of(slug + ":" + m.FRankId);
            m.Leader = OZ_Roles.IsLeader(uids[i]);
            m.Online = OZ_ChatWho.Online(uids[i]) != null;
            m.Me     = uids[i] == uid;
            st.Members.Insert(m);
        }

        // Кандидати на запрошення -- лише лідерові: друзі поза фракцією.
        if (st.MeLeader)
        {
            OZ_PlayerData me = OZ_PlayerStore.Load(uid);
            for (int f = 0; f < me.Friends.Count(); f++)
            {
                if (OZ_Factions.OfUid(me.Friends[f]) == slug)
                    continue;
                OZ_PlayerData fd = OZ_PlayerStore.Load(me.Friends[f]);
                if (fd.Name != "")
                    st.Candidates.Insert(fd.Name);
            }
        }

        return Serialise(st, ok, error);
    }

    private string Serialise(OZ_FactionState st, out bool ok, out string error)
    {
        string outJson;
        string err;
        if (!JsonFileLoader<OZ_FactionState>.MakeData(st, outJson, err, false))
        {
            ok = false;
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }
}

// Розголос «ролі змінились»: обидві сторінки, яким не байдуже, чують
// push і перечитують стан самі.
class OZ_PdaRolePush
{
    static void Changed(string uid)
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        for (int i = 0; i < players.Count(); i++)
        {
            if (!players[i])
                continue;
            PlayerIdentity id = players[i].GetIdentity();
            if (!id)
                continue;
            OZ_Rpc.Respond(id, OZ_PdaConst.PAGE_FACTION, "push", true, "", "");
            OZ_Rpc.Respond(id, OZ_PdaConst.PAGE_CONTACTS, "push", true, "", "");
        }
    }
}
