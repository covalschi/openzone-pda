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

    private ref OZ_MapState m_State;
    private bool m_Centred = false;

    // Обрана мітка. Порожньо -- нічого не обрано, і кнопка ставить нову.
    private string m_PickedId = "";

    // Куди поставити наступну мітку. Заповнюється кліком по порожньому місцю.
    private string m_PendingPos = "";

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
    }

    override void OnSelected()
    {
        // Відмова на дію, якої гравець уже не пам'ятає, ні до чого:
        // вкладку перемкнули -- тримати підказку більше нема сенсу.
        ClearHintHold();
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

    override bool OnPageClick(Widget w, int x, int y)
    {
        if (!w)
            return false;

        if (w == m_BtnCenter)
        {
            CentreOnSelf();
            return true;
        }

        // Клік по самій карті: спершу питаємо, чи не влучив він у наявну
        // мітку. Це має бути ПЕРШЕ питання -- інакше кожна спроба обрати
        // мітку ставила б поверх неї нову.
        if (w == m_Map)
        {
            MapClick(x, y);
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
            return "faction";
        if (cur == "faction")
            return "contacts";
        return "off";
    }

    // Куди клікнули у світових координатах, і що там уже стоїть.
    private void MapClick(int x, int y)
    {
        if (!m_Map || !m_State)
            return;

        vector at = m_Map.ScreenToMap(Vector(x, y, 0));

        string hit = MarkerNear(at);
        if (hit != "")
        {
            m_PickedId = hit;
            PaintMarkButton();
            return;
        }

        // Порожнє місце -- знімаємо вибір і запам'ятовуємо точку. Ставить
        // мітку кнопка, а не сам клік: інакше кожен зсув карти лишав би по
        // мітці на кожному випадковому натисканні.
        m_PickedId = "";
        m_PendingPos = at.ToString(false);
        PaintMarkButton();
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
        // Нічого не клікнули -- ставимо ТУТ. «Позначити місце, де я стою» --
        // найчастіша дія в Зоні, і вимагати заради неї влучити мишею в свою ж
        // позначку було б знущанням. Клік по карті лишається уточненням.
        string at = m_PendingPos;
        if (at == "" && m_State)
            at = m_State.SelfPos;

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
        if (!m_Map || !m_State || m_State.SelfPos == "")
            return;

        m_Map.SetMapPos(m_State.SelfPos.ToVector());
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        if (op == "transponder" || op == "marker_add" || op == "marker_del")
        {
            if (!ok)
            {
                SetHintSticky("MapHint", "#" + error);
            }
            else if (op == "marker_add")
            {
                // Поставили -- поле підпису чистимо, інакше наступна мітка
                // мовчки успадкує чужу назву.
                m_PendingPos = "";
                if (m_Name)
                    m_Name.SetText("");
            }
            else if (op == "marker_del")
            {
                m_PickedId = "";
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

            // Перше відкриття -- показуємо гравцеві, де він. Далі карта
            // лишається там, куди її поставив він сам.
            if (!m_Centred && m_State.SelfPos != "")
            {
                m_Centred = true;
                m_Map.SetMapPos(m_State.SelfPos.ToVector());
                m_Map.SetScale(0.35);
            }
        }

        PaintMarkButton();
        SetHint("MapHint", Hint());
    }

    // Три різні «нікого не видно», і гравець мусить розрізняти їх:
    // немає антени -- слухати нема чим;
    // антена є, нікого немає -- нікого й немає;
    // антена є, хтось є -- скільки саме.
    private string Hint()
    {
        if (!m_State.HasAntenna)
        {
            string noAnt = Marks();
            noAnt += "   ";
            noAnt += "#STR_OZ_MAP_NO_ANTENNA";
            return noAnt;
        }

        int n = 0;
        if (m_State.Beacons)
            n = m_State.Beacons.Count();

        int km = Math.Round(m_State.AntennaRangeM);

        string s = Marks();
        s += "   ";
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
        if (mode == "friends")
            return "#STR_OZ_TRANS_FRIENDS";
        if (mode == "contacts")
            return "#STR_OZ_TRANS_CONTACTS";
        if (mode == "faction")
            return "#STR_OZ_TRANS_FACTION";
        return "#STR_OZ_TRANS_OFF";
    }
}
