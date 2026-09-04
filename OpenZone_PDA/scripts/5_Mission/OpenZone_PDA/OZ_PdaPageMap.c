// Сторінка «Карта»: своя позиція, чужі маячки, перемикач транспондера.
//
// Масштаб і зсув веде сам MapWidget -- колесо й перетягування працюють без
// нашого коду. Наше -- позначки й те, коли карту центрувати.
//
// Центрування ОДНОРАЗОВЕ, при першому відкритті. Тягнути карту назад на
// гравця щосекунди означало б не дати її роздивитись.

class OZ_PdaPageMap : OZ_PdaPage
{
    private MapWidget m_Map;
    private ButtonWidget m_BtnMode;
    private ButtonWidget m_BtnCenter;

    private EditBoxWidget m_Name;
    private ButtonWidget m_BtnMark;

    // Список міток: перемикач, панель, рядки, поля редагування.
    private ButtonWidget           m_BtnList;
    private Widget                 m_Panel;
    private Widget                 m_Rows;
    private EditBoxWidget          m_EditName;
    private MultilineEditBoxWidget m_EditDesc;
    private ButtonWidget           m_BtnSave;
    private ButtonWidget           m_BtnShare;
    private ButtonWidget           m_BtnToCar;
    private ButtonWidget           m_BtnDel;
    private ref array<Widget>      m_RowWgts;
    private ButtonWidget m_BtnTrack;
    private ButtonWidget m_BtnRouteAdd;
    private ButtonWidget m_BtnRouteClear;
    private ButtonWidget m_BtnRouteGo;
    private ButtonWidget m_BtnRouteToCar;
    private bool                   m_ListOpen = false;

    // Підпис списку з минулого разу. Стан приходить ЩОСЕКУНДИ (маячки
    // рухаються), а перебудовувати рядки щосекунди означало б скидати скрол
    // і мигтіти. Рядки перебудовуються лише коли мітки СПРАВДІ змінились.
    private string m_RowsSig = "~";

    // Точка натискання миші. Клік приходить після відпускання, і лише
    // порівнявши його з точкою натискання можна відрізнити «клацнув» від
    // «потягнув карту». -1 означає «натискання не бачили» -- тоді кліку
    // віримо (програмний клік не має натискання, а обманювати нема кому).
    private int m_DownX = -1;
    private int m_DownY = -1;
    private static const int DRAG_SLOP_PX = 12;

    // Повний розмір карти, знятий у момент побудови. Одиниці SetSize --
    // ті самі, що поверне GetSize, і саме тому міряємо, а не вписуємо
    // константу з розкладки: розкладка масштабується.
    private float m_MapW;
    private float m_MapH;

    // Скільки ширини лишається карті при відкритому списку: 948 з 1282
    // одиниць розкладки. Панель шириною 325 починається на 957, тобто між
    // стиснутою картою й списком лишається 9 одиниць повітря.
    private static const float LIST_SQUEEZE = 0.7397;

    // АДМІНСЬКИХ КНОПОК ТУТ БІЛЬШЕ НЕМАЄ.
    //
    // SET SPAWN i CLEAR ZONE переїхали на панель SPAWNS у вкладцi VPP
    // (рiшення власника 2026-09-01: «КПК взагалi не повинен мати адмiнських
    // функцiй»). Знято ПIСЛЯ того, як панель навчилась усього, що вмiли цi
    // двi: слаг органiзацiї, слаг базової фракцiї, стейджинґ, запасну зону,
    // особисту точку й видиму вiдмову. Ранiше -- означало б лишити сервер
    // без ЄДИНОГО способу поставити зону.
    //
    // Разом iз ними пiшло подвiйне призначення поля MarkName: воно знову
    // просто iм'я мiтки, i нiчого бiльше.

    private ref OZ_MapState m_State;
    private bool m_Centred = false;

    // Обрана мітка. Порожньо -- нічого не обрано, і кнопка ставить нову.
    private string m_PickedId = "";

    // Ванільні іконки: своя й чужа мітки мусять відрізнятись з першого
    // погляду, і кольором тут не обійтись -- на карті кольорів і так вистачає.
    private static const string ICON_SELF   = "\\DZ\\gear\\navigation\\data\\map_tshelter_ca.paa";
    private static const string ICON_BEACON = "\\DZ\\gear\\navigation\\data\\map_transmitter_ca.paa";
    private static const string ICON_MARK   = "\\DZ\\gear\\navigation\\data\\map_tsign_ca.paa";

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_map.layout";
    }

    override void OnBuilt()
    {
        m_Map       = MapWidget.Cast(Wgt("Map"));
        m_BtnCenter = ButtonWidget.Cast(Wgt("BtnCenter"));
        m_BtnMode   = ButtonWidget.Cast(Wgt("BtnMode"));
        m_Name      = EditBoxWidget.Cast(Wgt("MarkName"));
        m_BtnMark   = ButtonWidget.Cast(Wgt("BtnMark"));

        SetText("BtnCenterText", "#STR_OZ_MAP_CENTER");

        m_BtnList  = ButtonWidget.Cast(Wgt("BtnList"));
        if (m_Map)
            m_Map.GetSize(m_MapW, m_MapH);
        m_Panel    = Wgt("MarkerPanel");
        m_Rows     = Wgt("MarkerRows");
        m_EditName = EditBoxWidget.Cast(Wgt("MarkerName"));
        m_EditDesc = MultilineEditBoxWidget.Cast(Wgt("MarkerDesc"));
        m_BtnSave  = ButtonWidget.Cast(Wgt("BtnMarkSave"));
        m_BtnShare = ButtonWidget.Cast(Wgt("BtnMarkShare"));
        SetText("BtnMarkShareText", "#STR_OZ_MAP_SHARE");
        m_BtnToCar = ButtonWidget.Cast(Wgt("BtnMarkToCar"));
        SetText("BtnMarkToCarText", "#STR_OZ_TO_CARRIER");
        m_BtnDel   = ButtonWidget.Cast(Wgt("BtnMarkDel"));
        m_BtnTrack = ButtonWidget.Cast(Wgt("BtnMarkTrack"));
        m_BtnRouteAdd   = ButtonWidget.Cast(Wgt("BtnRouteAdd"));
        SetText("BtnRouteAddText", "#STR_OZ_ROUTE_ADD");
        m_BtnRouteClear = ButtonWidget.Cast(Wgt("BtnRouteClear"));
        SetText("BtnRouteClearText", "#STR_OZ_ROUTE_CLEAR");
        m_BtnRouteGo    = ButtonWidget.Cast(Wgt("BtnRouteGo"));
        m_BtnRouteToCar = ButtonWidget.Cast(Wgt("BtnRouteToCar"));
        SetText("BtnRouteToCarText", "#STR_OZ_ROUTE_TO_CAR");
        m_RowWgts  = new array<Widget>();

        SetText("BtnListText", "#STR_OZ_MAP_LIST");
        SetText("BtnMarkSaveText", "#STR_OZ_MAP_SAVE");
        SetText("BtnMarkDelText", "#STR_OZ_MAP_DELETE");
    }

    // Точку натискання пам'ятаємо ЛИШЕ для карти: решті віджетів вона ні до
    // чого, а подію мусить побачити й рушій -- тому false завжди.
    override bool OnPageMouseDown(Widget w, int x, int y)
    {
        if (w == m_Map)
        {
            m_DownX = x;
            m_DownY = y;
        }
        return false;
    }

    // «Клік» по карті живе на відпусканні: OnClick MapWidget не породжує
    // (зміряно; ваніль слухає OnDoubleClick обробником на самому віджеті).
    // false завжди -- рушію відпускання потрібне для його власного стану.
    override bool OnPageMouseUp(Widget w, int x, int y)
    {
        // Клік -- це НАТИСКАННЯ НА КАРТІ плюс відпускання на ній же. Без
        // цієї умови відпускання миші, натиснутої деінде (кнопка, край
        // панелі), ставило мітку там, де палець зіслизнув з карти.
        if (w == m_Map && m_DownX >= 0)
            MapClick(x, y);
        return false;
    }

    override void OnSelected()
    {
        // Відмова на дію, якої гравець уже не пам'ятає, ні до чого:
        // вкладку перемкнули -- тримати підказку більше нема сенсу.
        ClearHintHold();
        Request();
    }

    // НЕ полить: стан питається при відкритті та після операцій (кожна
    // операція й так перепитує сама), а маячки сервер ПУШИТЬ окремим
    // конвертом -- аудит 2026-08-30 нарахував ~59 зайвих запитів на
    // хвилину відкритої карти на гравця.
    override void OnRefresh()
    {
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "state", "{}");
    }

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        if (w == m_BtnCenter)
        {
            CentreOnSelf();
            return true;
        }

        // Кліка по самій карті ТУТ немає: MapWidget -- не кнопка, OnClick
        // по ньому не приходить (зміряно). Він збирається в OnPageMouseUp.

        // Зони спавна більше не ставлять звідси -- див. коментар угорі про
        // те, куди вони поїхали й чому саме в цьому порядку.

        if (w == m_BtnList)
        {
            m_ListOpen = !m_ListOpen;
            if (m_Panel)
                m_Panel.Show(m_ListOpen);

            // Карта СТИСКАЄТЬСЯ, а не ховається під панель: MapWidget малює
            // себе поверх усього -- і сусідів, і власних дітей, незалежно
            // від priority. Виміряно на стенді: кнопка з priority 4 над
            // картою не малювалась узагалі. Тож поверх карти не малює НІХТО,
            // і місце списку звільняє сама карта.
            if (m_Map && m_MapW > 0)
            {
                if (m_ListOpen)
                    m_Map.SetSize(m_MapW * LIST_SQUEEZE, m_MapH);
                else
                    m_Map.SetSize(m_MapW, m_MapH);
            }

            if (m_ListOpen)
                RebuildRows(true);
            return true;
        }

        // Рядок списку міток. Ім'я віджета -- це Id мітки, див. RebuildRows.
        if (w.GetUserID() == 5)
        {
            Pick(w.GetName());
            return true;
        }

        if (w == m_BtnSave)
        {
            SendMarkerEdit();
            return true;
        }

        if (w == m_BtnToCar)
        {
            if (m_PickedId == "")
            {
                SetHintSticky("MapHint", "#STR_OZ_MAP_PICK_FIRST");
                return true;
            }

            OZ_MarkerRef cref = new OZ_MarkerRef();
            cref.Id = m_PickedId;

            string cjson;
            string cerr;
            if (JsonFileLoader<OZ_MarkerRef>.MakeData(cref, cjson, cerr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "carrier_add", cjson);
            return true;
        }

        if (w == m_BtnShare)
        {
            if (m_PickedId == "")
            {
                SetHintSticky("MapHint", "#STR_OZ_MAP_PICK_FIRST");
                return true;
            }

            OZ_MapMarker mk = FindMarker(m_PickedId);
            if (!mk)
                return true;

            // Формат розбирає одержувач: "[MARK] назва @ x z — опис".
            vector mp = mk.Pos.ToVector();
            int sx = Math.Round(mp[0]);
            int sz = Math.Round(mp[2]);

            string line = "[MARK] " + mk.Name + " @ " + sx.ToString() + " " + sz.ToString();
            // Розділювач ASCII навмисно: типографське тире губилось десь
            // між EditBox і відправкою, і опис не доїжджав (живий тест
            // 2026-08-29). Парсер розуміє обидва написання.
            if (mk.Desc != "")
                line += " -- " + mk.Desc;

            OZ_PdaCompose.Put(line);

            OZ_PdaMenu menu = OZ_PdaMenu.Cast(GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA));
            if (menu)
                menu.Select(OZ_PdaConst.PAGE_CHAT);
            return true;
        }

        if (w == m_BtnRouteAdd)
        {
            if (m_PickedId == "")
            {
                SetHintSticky("MapHint", "#STR_OZ_MAP_PICK_FIRST");
                return true;
            }

            OZ_MarkerRef ra = new OZ_MarkerRef();
            ra.Id = m_PickedId;

            string raj;
            string rerr;
            if (JsonFileLoader<OZ_MarkerRef>.MakeData(ra, raj, rerr, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "route_add", raj);
            return true;
        }

        if (w == m_BtnRouteClear)
        {
            OZ_PdaRoute.Stop();
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "route_clear", "{}");
            return true;
        }

        if (w == m_BtnRouteGo)
        {
            // Неактивний -- АКТИВУВАТИ; активний -- ПРОЙДЕНО (ручна
            // позначка поточної точки). Кінець нитки гасить її сам.
            if (OZ_PdaRoute.Active)
            {
                OZ_PdaRoute.Advance();
            }
            else
            {
                if (!m_State || !m_State.Route || m_State.Route.Count() == 0)
                {
                    SetHintSticky("MapHint", "#STR_OZ_ERR_ROUTE_EMPTY");
                    return true;
                }
                OZ_PdaRoute.Start(m_State.Route);
            }
            RebuildRows(true);
            return true;
        }

        if (w == m_BtnRouteToCar)
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "route_write", "{}");
            return true;
        }

        if (w == m_BtnTrack)
        {
            if (m_PickedId == "")
            {
                SetHintSticky("MapHint", "#STR_OZ_MAP_PICK_FIRST");
                return true;
            }

            // Той самий клік знімає ведення з уже веденої мітки.
            if (OZ_PdaTrack.Id == m_PickedId)
            {
                OZ_PdaTrack.Id   = "";
                OZ_PdaTrack.Name = "";
                OZ_PdaTrack.Point  = "";
            }
            else
            {
                OZ_MapMarker tmk = FindMarker(m_PickedId);
                if (tmk)
                {
                    OZ_PdaTrack.Id   = tmk.Id;
                    OZ_PdaTrack.Name = tmk.Name;
                    OZ_PdaTrack.Point  = tmk.Pos;
                }
            }

            RebuildRows(true);
            return true;
        }

        if (w == m_BtnDel)
        {
            if (m_PickedId == "")
            {
                SetHintSticky("MapHint", "#STR_OZ_MAP_PICK_FIRST");
                return true;
            }
            SendMarkerDelete();
            return true;
        }

        if (w == m_BtnMark)
        {
            if (m_PickedId != "")
                SendMarkerDelete();
            else
                SendMarkerAdd();
            return true;
        }

        if (w == m_BtnMode)
        {
            OZ_TransponderOp op = new OZ_TransponderOp();
            NextSet(op.Set);

            string json;
            string err;
            if (JsonFileLoader<OZ_TransponderOp>.MakeData(op, json, err, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "transponder", json);
            return true;
        }

        return false;
    }

    // Коло від найтихішого до найгучнішого (ТЗ-4 R-A3.1, R-A3.2):
    //   off -> contacts -> faction -> faction+contacts -> public -> off.
    // Випадкове натискання підвищує гучність на один крок, а не вмикає одразу
    // «всім». Кроки з "faction" існують лише тоді, коли сервер сказав, що
    // мод фракцій є (R-A3.4): без нього перемикача немає, а не «не працює».
    private string SetKey(array<string> s)
    {
        if (!s || s.Count() == 0)
            return "off";
        if (s.Find("public") != -1)
            return "public";
        bool f = s.Find("faction") != -1;
        bool c = s.Find("contacts") != -1;
        if (f && c)
            return "both";
        if (f)
            return "faction";
        if (c)
            return "contacts";
        return "off";
    }

    private void NextSet(array<string> outSet)
    {
        string cur = "off";
        bool factions = false;
        if (m_State)
        {
            cur = SetKey(m_State.TransponderSet);
            factions = m_State.FactionsPresent;
        }

        outSet.Clear();

        if (cur == "off")
        {
            outSet.Insert("contacts");
            return;
        }
        if (cur == "contacts")
        {
            if (factions)
            {
                outSet.Insert("faction");
                return;
            }
            outSet.Insert("public");
            return;
        }
        if (cur == "faction")
        {
            outSet.Insert("faction");
            outSet.Insert("contacts");
            return;
        }
        if (cur == "both")
        {
            outSet.Insert("public");
            return;
        }
        // "public" -> off: порожній набір.
    }

    // Куди клікнули у світових координатах, і що там уже стоїть.
    private void MapClick(int x, int y)
    {
        if (!m_Map || !m_State)
            return;

        // ПЕРЕТЯГУВАННЯ -- НЕ КЛІК. Клік приходить після відпускання, тобто
        // й наприкінці кожного зсуву карти теж. Якби він ставив мітку, кожен
        // зсув лишав би по мітці там, де палець відпустив. Порівнюємо з
        // точкою натискання: зсунулись далі за поріг -- це був зсув.
        if (m_DownX >= 0)
        {
            int moved = Math.AbsInt(x - m_DownX) + Math.AbsInt(y - m_DownY);
            m_DownX = -1;
            m_DownY = -1;
            if (moved > DRAG_SLOP_PX)
                return;
        }

        vector at = m_Map.ScreenToMap(Vector(x, y, 0));

        string hit = MarkerNear(at);
        if (hit != "")
        {
            // Повторний клік по обраній -- зняти вибір. Інакше «нічого не
            // обрано» не досягалося б узагалі.
            if (hit == m_PickedId)
                Pick("");
            else
                Pick(hit);
            return;
        }

        // Порожнє місце -- СТАВИМО МІТКУ ТУТ, одразу. Назва з поля внизу,
        // опис додається потім через панель. Промах коштує два кліки:
        // обрати й видалити.
        OZ_MapMarker m = new OZ_MapMarker();
        m.Pos = at.ToString(false);
        if (m_Name)
            m.Name = m_Name.GetText();

        string json;
        string err;
        if (JsonFileLoader<OZ_MapMarker>.MakeData(m, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "marker_add", json);
    }

    // Обрати мітку (або зняти вибір порожнім id): підсвітити на карті й у
    // списку, заповнити поля редагування, показати карті ДЕ вона.
    private void Pick(string id)
    {
        m_PickedId = id;

        OZ_MapMarker m = FindMarker(id);
        if (m)
        {
            if (m_EditName)
                m_EditName.SetText(m.Name);
            if (m_EditDesc)
                m_EditDesc.SetText(m.Desc);
            if (m_Map)
                m_Map.SetMapPos(m.Pos.ToVector());
        }
        else
        {
            if (m_EditName)
                m_EditName.SetText("");
            if (m_EditDesc)
                m_EditDesc.SetText("");
        }

        PaintMarkButton();
        Paint();
        RebuildRows(true);
    }

    private OZ_MapMarker FindMarker(string id)
    {
        if (id == "" || !m_State || !m_State.Markers)
            return null;

        for (int i = 0; i < m_State.Markers.Count(); i++)
        {
            if (m_State.Markers[i].Id == id)
                return m_State.Markers[i];
        }
        return null;
    }

    private void SendMarkerEdit()
    {
        if (m_PickedId == "")
        {
            SetHintSticky("MapHint", "#STR_OZ_MAP_PICK_FIRST");
            return;
        }

        OZ_MapMarker m = new OZ_MapMarker();
        m.Id = m_PickedId;
        if (m_EditName)
            m.Name = m_EditName.GetText();
        if (m_EditDesc)
        {
            // GetText багаторядкового поля пише в out-параметр, а не
            // повертає: він успадкований від іншого прото, ніж у EditBox.
            // Через розклейку: опис теж обростав переносами посеред слів.
            string d = OZ_Unwrap.Read(m_EditDesc, TextWidget.Cast(Wgt("DescRuler")));
            m.Desc = d;
        }

        string json;
        string err;
        if (JsonFileLoader<OZ_MapMarker>.MakeData(m, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "marker_edit", json);
    }

    // Спрайт рядка мітки. OZ_MapMarker (OZ_PdaTypes.c) поки несе лише
    // Id/Name/Pos/Desc -- поля виду немає, тож завжди базова крапка;
    // mk_stash/mk_danger/mk_route чекають на це поле в мітці.
    private string SpriteFor(OZ_MapMarker m)
    {
        return "mk_marker";
    }

    // Перебудувати рядки списку. force -- перебудувати завжди (вибір
    // змінився, підсвітку треба перемалювати); без force -- лише коли
    // самі мітки змінилися, бо стан приходить щосекунди.
    private void RebuildRows(bool force)
    {
        if (!m_Rows || !m_ListOpen)
            return;

        string sig = "";
        if (m_State && m_State.Markers)
        {
            for (int i = 0; i < m_State.Markers.Count(); i++)
            {
                OZ_MapMarker mk = m_State.Markers[i];
                sig += mk.Id + "|" + mk.Name + "|" + mk.Desc + ";";
            }
        }
        sig += "@" + m_PickedId + "#" + OZ_PdaTrack.Id;
        if (m_State && m_State.Route)
            sig += "$" + m_State.Route.Count().ToString();
        sig += "&" + OZ_PdaRoute.At.ToString() + OZ_PdaRoute.Active.ToString();

        if (!force && sig == m_RowsSig)
            return;
        m_RowsSig = sig;

        for (int r = 0; r < m_RowWgts.Count(); r++)
        {
            if (m_RowWgts[r])
                m_RowWgts[r].Unlink();
        }
        m_RowWgts.Clear();
        m_Rows.Update();

        int n = 0;
        int limit = 0;
        if (m_State)
        {
            if (m_State.Markers)
                n = m_State.Markers.Count();
            limit = m_State.MarkerLimit;
        }
        SetText("SecMarksLbl", Widget.TranslateString("#STR_OZ_MAP_LIST") + "  " + n.ToString() + "/" + limit.ToString());

        // Гравець, від якого міряються відстані. Дистанція в списку --
        // знімок на мить перемальовування; ЖИВА цифра веденої мітки живе
        // під мінікартою.
        vector meAt = vector.Zero;
        PlayerBase mePl = PlayerBase.Cast(GetGame().GetPlayer());
        if (mePl)
            meAt = mePl.GetPosition();

        for (int k = 0; k < n; k++)
        {
            OZ_MapMarker mrk = m_State.Markers[k];

            Widget row = GetGame().GetWorkspace().CreateWidgets("OpenZone_PDA/gui/layouts/oz_pda_marker_row.layout", m_Rows);
            if (!row)
                break;

            // Ім'я віджета -- Id мітки: саме його читає OnClick.
            row.SetName(mrk.Id);
            row.SetUserID(5);
            m_RowWgts.Insert(row);

            ImageWidget ic = ImageWidget.Cast(row.FindAnyWidget("RowIcon"));
            if (ic)
                ic.LoadImageFile(0, "set:oz_pda_icons image:" + SpriteFor(mrk));

            TextWidget name = TextWidget.Cast(row.FindAnyWidget("RowName"));
            if (name)
            {
                if (mrk.Name != "")
                    name.SetText(mrk.Name);
                else
                    name.SetText("#STR_OZ_MAP_UNNAMED");
            }

            TextWidget where = TextWidget.Cast(row.FindAnyWidget("RowWhere"));
            if (where)
            {
                vector p = mrk.Pos.ToVector();
                int px = Math.Round(p[0]);
                int pz = Math.Round(p[2]);

                string wtxt = px.ToString() + " " + pz.ToString();
                if (mePl && GpsKnows())
                {
                    int dm = Math.Round(vector.Distance(Vector(meAt[0], 0, meAt[2]), Vector(p[0], 0, p[2])));
                    wtxt += "  " + dm.ToString() + " m";
                }
                if (mrk.Id == OZ_PdaTrack.Id)
                    wtxt = ">> " + wtxt;
                where.SetText(wtxt);
            }

            // Опис у рядку РІЖЕТЬСЯ: сервер дозволяє 160 байтів
            // (MarkerDescMaxBytes), а другий рядок тримає 261 одиницю, тобто
            // 33 українські літери у 13 pt (зміряно на стенді 2026-09-05:
            // 33 літери = 257.8). 64 байти -- це 32 літери, і вони влазять із
            // запасом. Хвіст не губиться: цілий опис показує поле MarkerDesc,
            // щойно рядок обрано. Clip ріже по межі символу, а не байта.
            TextWidget desc = TextWidget.Cast(row.FindAnyWidget("RowDesc"));
            if (desc)
                desc.SetText(OZ_Text.Clip(mrk.Desc, 64));

            Widget pick = row.FindAnyWidget("RowPick");
            if (pick)
                pick.Show(mrk.Id == m_PickedId);
        }

        // Стопка складається САМА: MarkerRows -- WrapSpacer із «Size To
        // Content V» 1, рядки в ньому пропорційні (size 1 42), і після
        // Update() його висота дорівнює сумі рядків. Ні SetPos, ні SetSize
        // тут більше немає. Старий коментар звинувачував спейсер у намотці
        // власного розміру (нібито 105 613 юнітів); зміряно наново на стенді
        // 2026-09-04 -- 30 рядків дають рівно 30 x 42, а скрол прокручує.
        m_Rows.Update();

        if (m_BtnTrack)
        {
            if (m_PickedId != "" && m_PickedId == OZ_PdaTrack.Id)
                SetText("BtnMarkTrackText", "#STR_OZ_MAP_UNTRACK");
            else
                SetText("BtnMarkTrackText", "#STR_OZ_MAP_TRACK");
        }

        int rpts = 0;
        if (m_State && m_State.Route)
            rpts = m_State.Route.Count();

        string rhead = Widget.TranslateString("#STR_OZ_ROUTE") + "  " + rpts.ToString();
        if (OZ_PdaRoute.Active)
            rhead += "  [" + (OZ_PdaRoute.At + 1).ToString() + "/" + OZ_PdaRoute.CountSafe().ToString() + "]";
        SetText("RouteHead", rhead);

        if (m_BtnRouteGo)
        {
            if (OZ_PdaRoute.Active)
                SetText("BtnRouteGoText", "#STR_OZ_ROUTE_PASS");
            else
                SetText("BtnRouteGoText", "#STR_OZ_ROUTE_GO");
        }
    }

    private string MarkerNear(vector at)
    {
        if (!m_State.Markers)
            return "";

        for (int i = 0; i < m_State.Markers.Count(); i++)
        {
            OZ_MapMarker m = m_State.Markers[i];
            vector p = m.Pos.ToVector();

            // По ПЛОЩИНІ: висота мітки й висота кліку по карті -- різні речі,
            // і додавати її у відстань означало б не влучати на схилах.
            float dx = p[0] - at[0];
            float dz = p[2] - at[2];
            if (Math.Sqrt(dx * dx + dz * dz) <= OZ_PdaConst.MARKER_PICK_M)
                return m.Id;
        }
        return "";
    }

    private void SendMarkerAdd()
    {
        // Кнопка ставить мітку ТАМ, ДЕ СТОЇШ. «Позначити місце, де я стою» --
        // найчастіша дія в Зоні. Мітку В ІНШОМУ місці ставить клік по карті.
        string at = "";
        if (m_State)
            at = LocalSelfPos();

        if (at == "")
        {
            SetHintSticky("MapHint", "#STR_OZ_MAP_MARK_HINT");
            return;
        }

        OZ_MapMarker m = new OZ_MapMarker();
        m.Pos = at;
        if (m_Name)
            m.Name = m_Name.GetText();

        string json;
        string err;
        if (JsonFileLoader<OZ_MapMarker>.MakeData(m, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "marker_add", json);
    }

    private void SendMarkerDelete()
    {
        OZ_MarkerRef r = new OZ_MarkerRef();
        r.Id = m_PickedId;

        string json;
        string err;
        if (JsonFileLoader<OZ_MarkerRef>.MakeData(r, json, err, false))
            OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "marker_del", json);
    }

    private void PaintMarkButton()
    {
        if (m_PickedId != "")
            SetText("BtnMarkText", "#STR_OZ_MAP_DELETE");
        else
            SetText("BtnMarkText", "#STR_OZ_MAP_MARK");
    }

    private void CentreOnSelf()
    {
        string selfPos = LocalSelfPos();
        if (!m_Map || selfPos == "")
            return;

        m_Map.SetMapPos(selfPos.ToVector());
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        // Пуш маячків: жива частина стану їде сама, поки сторінка відкрита.
        if (op == "beacons" && ok)
        {
            OZ_BeaconPush bp;
            string berr;
            if (JsonFileLoader<OZ_BeaconPush>.LoadData(json, bp, berr) && bp && m_State)
            {
                m_State.Beacons = bp.Beacons;
                Paint();
            }
            return;
        }

        if (op == "route_add" || op == "route_clear" || op == "route_write" || op == "route_take")
        {
            if (!ok)
                SetHintSticky("MapHint", "#" + error);
            else if (op == "route_write")
                SetHintSticky("MapHint", "#STR_OZ_CARRIER_SAVED");
            Request();
            return;
        }

        if (op == "carrier_add")
        {
            if (ok)
                SetHintSticky("MapHint", "#STR_OZ_CARRIER_SAVED");
            else
                SetHintSticky("MapHint", "#" + error);
            return;
        }

        if (op == "transponder" || op == "marker_add" || op == "marker_del" || op == "marker_edit")
        {
            if (!ok)
            {
                SetHintSticky("MapHint", "#" + error);
            }
            else if (op == "marker_add")
            {
                // Поставили -- поле підпису чистимо, інакше наступна мітка
                // мовчки успадкує чужу назву.
                if (m_Name)
                    m_Name.SetText("");
            }
            else if (op == "marker_del")
            {
                Pick("");
            }

            PaintMarkButton();
            Request();
            return;
        }

        if (op != "state")
            return;

        if (!ok)
        {
            SetHintSticky("MapHint", "#" + error);
            return;
        }

        string err;
        OZ_MapState st;
        if (!JsonFileLoader<OZ_MapState>.LoadData(json, st, err))
        {
            OZ_Log.Error("map state unreadable: " + err);
            return;
        }

        m_State = st;
        Paint();
    }

    // Своя позиція клієнтові відома без сервера; з відповіді вона
    // прибрана з ужитку -- інакше точка «ти тут» жила б минулою секундою.
    private string LocalSelfPos()
    {
        PlayerBase me = PlayerBase.Cast(GetGame().GetPlayer());
        if (!me)
            return "";
        return me.GetPosition().ToString(false);
    }

    private void Paint()
    {
        SetText("BtnModeText", ModeLabel(SetKey(m_State.TransponderSet)));
        string selfPos = LocalSelfPos();

        if (m_Map)
        {
            // Стираємо ВСЕ й малюємо заново: маячки рухаються, і додавати
            // поверх старих означало б лишити на карті сліди там, де людини
            // вже немає.
            m_Map.ClearUserMarks();

            // AddUserMark НЕ розгортає ключі перекладу: те, що SetText
            // показав би як «you», тут вилізло б на карту як
            // #STR_OZ_MAP_YOU. Розгортаємо самі -- саме для цього
            // Widget.TranslateString і є.
            if (selfPos != "" && GpsKnows())
                m_Map.AddUserMark(selfPos.ToVector(), Widget.TranslateString("#STR_OZ_MAP_YOU"), ARGB(255, 255, 122, 26), ICON_SELF);

            for (int i = 0; m_State.Beacons && i < m_State.Beacons.Count(); i++)
            {
                OZ_MapBeacon b = m_State.Beacons[i];
                m_Map.AddUserMark(b.Pos.ToVector(), b.Name, ARGB(255, 126, 200, 160), ICON_BEACON);
            }

            for (int k = 0; m_State.Markers && k < m_State.Markers.Count(); k++)
            {
                OZ_MapMarker m = m_State.Markers[k];

                // Обрана мітка світиться -- інакше після кліку не видно, яку
                // саме зараз видалить кнопка.
                int colour = ARGB(255, 214, 214, 222);
                if (m.Id == m_PickedId)
                    colour = ARGB(255, 255, 122, 26);

                m_Map.AddUserMark(m.Pos.ToVector(), m.Name, colour, ICON_MARK);
            }

            for (int rr = 0; m_State.Route && rr < m_State.Route.Count(); rr++)
            {
                OZ_MapMarker rm = m_State.Route[rr];
                // Нитка нумерована прямо в підписі: порядок і є маршрут.
                m_Map.AddUserMark(rm.Pos.ToVector(), (rr + 1).ToString() + ". " + rm.Name, ARGB(255, 255, 170, 80), ICON_MARK);
            }

            // Перше відкриття -- показуємо гравцеві, де він. Далі карта
            // лишається там, куди її поставив він сам.
            if (!m_Centred && selfPos != "")
            {
                m_Centred = true;
                m_Map.SetMapPos(selfPos.ToVector());
                m_Map.SetScale(0.35);
            }
        }

        PaintMarkButton();
        RebuildRows(false);
        SetHint("MapHint", Hint());
    }

    // Три різні «нікого не видно», і гравець мусить розрізняти їх:
    // немає антени -- слухати нема чим;
    // антена є, нікого немає -- нікого й немає;
    // антена є, хтось є -- скільки саме.
    private string Hint()
    {
        // Капсула: світ у ній зупинився (ТЗ-4 R-B1) -- ні живих маячків, ні
        // «ти тут»; є лише записане на приладі.
        if (m_State.Frozen)
        {
            string cap = Marks();
            cap += "   ";
            cap += "#STR_OZ_MAP_CAPSULE";
            return cap;
        }

        // Без GPS прилад не знає, де він (R-B2.4): карта є, мітки й маршрут
        // є, а «ти тут» і відстаней немає -- і про це кажуть словом.
        string gps = "";
        if (!m_State.HasGps)
        {
            gps = "#STR_OZ_MAP_NO_GPS";
            gps += "   ";
        }

        if (!m_State.HasAntenna)
        {
            string noAnt = Marks();
            noAnt += "   ";
            noAnt += gps;
            noAnt += "#STR_OZ_MAP_NO_ANTENNA";
            return noAnt;
        }

        int n = 0;
        if (m_State.Beacons)
            n = m_State.Beacons.Count();

        int km = Math.Round(m_State.AntennaRangeM);

        string s = Marks();
        s += "   ";
        s += gps;
        s += "#STR_OZ_MAP_RANGE";
        s += "  " + km.ToString() + " m";

        if (n == 0)
        {
            s += "   ";
            s += "#STR_OZ_MAP_NOBODY";
            return s;
        }

        s += "   ";
        s += "#STR_OZ_MAP_BEACONS";
        s += "  " + n.ToString();
        return s;
    }

    // «Ти тут» і відстані -- лише коли прилад знає, де він (GPS), і не є
    // капсулою (ТЗ-4 R-B1.1, R-B2.2).
    private bool GpsKnows()
    {
        if (!m_State)
            return false;
        return m_State.HasGps && !m_State.Frozen;
    }

    private string Marks()
    {
        int have = 0;
        if (m_State.Markers)
            have = m_State.Markers.Count();

        string s = "#STR_OZ_MAP_MARKS";
        s += "  " + have.ToString();
        s += "/" + m_State.MarkerLimit.ToString();
        return s;
    }

    private string ModeLabel(string mode)
    {
        if (mode == "public")
            return "#STR_OZ_TRANS_PUBLIC";
        if (mode == "contacts")
            return "#STR_OZ_TRANS_CONTACTS";
        if (mode == "faction")
            return "#STR_OZ_TRANS_FACTION";
        if (mode == "both")
            return "#STR_OZ_TRANS_BOTH";
        return "#STR_OZ_TRANS_OFF";
    }
}
