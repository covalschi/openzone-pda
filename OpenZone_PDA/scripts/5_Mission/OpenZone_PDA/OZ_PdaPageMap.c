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

    private ref OZ_MapState m_State;
    private bool m_Centred = false;

    // Ванільні іконки: своя й чужа мітки мусять відрізнятись з першого
    // погляду, і кольором тут не обійтись -- на карті кольорів і так вистачає.
    private static const string ICON_SELF   = "\\DZ\\gear\\navigation\\data\\map_tshelter_ca.paa";
    private static const string ICON_BEACON = "\\DZ\\gear\\navigation\\data\\map_transmitter_ca.paa";

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_map.layout";
    }

    override void OnBuilt()
    {
        m_Map       = MapWidget.Cast(Wgt("Map"));
        m_BtnCenter = ButtonWidget.Cast(Wgt("BtnCenter"));
        m_BtnMode   = ButtonWidget.Cast(Wgt("BtnMode"));

        SetText("BtnCenterText", "#STR_OZ_MAP_CENTER");
    }

    override void OnSelected()
    {
        Request();
    }

    // Раз на секунду: чужі маячки рухаються самі.
    override void OnRefresh()
    {
        Request();
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "state", "{}");
    }

    override bool OnPageClick(Widget w)
    {
        if (!w)
            return false;

        if (w == m_BtnCenter)
        {
            CentreOnSelf();
            return true;
        }

        if (w == m_BtnMode)
        {
            OZ_TransponderOp op = new OZ_TransponderOp();
            op.Mode = NextMode();

            string json;
            string err;
            if (JsonFileLoader<OZ_TransponderOp>.MakeData(op, json, err, false))
                OZ_Rpc.Request(OZ_PdaConst.PAGE_MAP, "transponder", json);
            return true;
        }

        return false;
    }

    // Коло off -> public -> friends -> contacts -> off. Порядок навмисно від
    // найтихішого до найгучнішого: випадкове натискання підвищує гучність на
    // один крок, а не вмикає одразу «всім».
    private string NextMode()
    {
        string cur = "off";
        if (m_State)
            cur = m_State.TransponderMode;

        if (cur == "off")
            return "public";
        if (cur == "public")
            return "friends";
        if (cur == "friends")
            return "contacts";
        return "off";
    }

    private void CentreOnSelf()
    {
        if (!m_Map || !m_State || m_State.SelfPos == "")
            return;

        m_Map.SetMapPos(m_State.SelfPos.ToVector());
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op == "transponder")
        {
            if (!ok)
                SetText("MapHint", "#" + error);
            Request();
            return;
        }

        if (op != "state")
            return;

        if (!ok)
        {
            SetText("MapHint", "#" + error);
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

    private void Paint()
    {
        SetText("BtnModeText", ModeLabel(m_State.TransponderMode));

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
            if (m_State.SelfPos != "")
                m_Map.AddUserMark(m_State.SelfPos.ToVector(), Widget.TranslateString("#STR_OZ_MAP_YOU"), ARGB(255, 255, 122, 26), ICON_SELF);

            for (int i = 0; m_State.Beacons && i < m_State.Beacons.Count(); i++)
            {
                OZ_MapBeacon b = m_State.Beacons[i];
                m_Map.AddUserMark(b.Pos.ToVector(), b.Name, ARGB(255, 126, 200, 160), ICON_BEACON);
            }

            // Перше відкриття -- показуємо гравцеві, де він. Далі карта
            // лишається там, куди її поставив він сам.
            if (!m_Centred && m_State.SelfPos != "")
            {
                m_Centred = true;
                m_Map.SetMapPos(m_State.SelfPos.ToVector());
                m_Map.SetScale(0.35);
            }
        }

        SetText("MapHint", Hint());
    }

    // Три різні «нікого не видно», і гравець мусить розрізняти їх:
    // немає антени -- слухати нема чим;
    // антена є, нікого немає -- нікого й немає;
    // антена є, хтось є -- скільки саме.
    private string Hint()
    {
        if (!m_State.HasAntenna)
            return "#STR_OZ_MAP_NO_ANTENNA";

        int n = 0;
        if (m_State.Beacons)
            n = m_State.Beacons.Count();

        int km = Math.Round(m_State.AntennaRangeM);

        string s = "#STR_OZ_MAP_RANGE";
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

    private string ModeLabel(string mode)
    {
        if (mode == "public")
            return "#STR_OZ_TRANS_PUBLIC";
        if (mode == "friends")
            return "#STR_OZ_TRANS_FRIENDS";
        if (mode == "contacts")
            return "#STR_OZ_TRANS_CONTACTS";
        return "#STR_OZ_TRANS_OFF";
    }
}
