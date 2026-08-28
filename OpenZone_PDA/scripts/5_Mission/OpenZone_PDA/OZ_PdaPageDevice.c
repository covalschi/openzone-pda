// Сторінка «Пристрій»: що це за КПК, чим живиться, що в нього вставлено.
//
// Усе, що вона показує, приходить одним запитом device/status. Клієнт нічого
// не обчислює сам -- інакше з'явилась би друга правда про стан пристрою.

class OZ_PdaPageDevice : OZ_PdaPage
{
    private ItemPreviewWidget m_Preview;
    private Widget m_ChargeFill;
    private ButtonWidget m_BtnPower;
    private ButtonWidget m_BtnPin;
    private ButtonWidget m_BtnPinClear;
    private ButtonWidget m_BtnAutoLock;

    private ref OZ_PdaDeviceStatus m_Status;

    // Кнопки носія. Видимість веде стан: без чипа їх немає, нечитаний не
    // імпортується, замкнений конфігом не пишеться й не стирається.
    private ButtonWidget m_BtnCarWriteM;
    private ButtonWidget m_BtnCarWriteN;
    private ButtonWidget m_BtnCarView;
    private ButtonWidget m_BtnCarErase;
    private Widget       m_CarPane;
    private ButtonWidget m_BtnCarDoImport;
    private ButtonWidget m_BtnCarClose;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_device.layout";
    }

    override void OnBuilt()
    {
        m_BtnCarWriteM   = ButtonWidget.Cast(Wgt("BtnCarWriteM"));
        m_BtnCarWriteN   = ButtonWidget.Cast(Wgt("BtnCarWriteN"));
        m_BtnCarView     = ButtonWidget.Cast(Wgt("BtnCarView"));
        m_BtnCarErase    = ButtonWidget.Cast(Wgt("BtnCarErase"));
        m_CarPane        = Wgt("CarPane");
        m_BtnCarDoImport = ButtonWidget.Cast(Wgt("BtnCarDoImport"));
        m_BtnCarClose    = ButtonWidget.Cast(Wgt("BtnCarClose"));
        SetText("BtnCarWriteMText", "#STR_OZ_DEV_WRITE_MARKS");
        SetText("BtnCarWriteNText", "#STR_OZ_DEV_WRITE_NOTES");
        SetText("BtnCarViewText", "#STR_OZ_DEV_CAR_VIEW");
        SetText("BtnCarEraseText", "#STR_OZ_DEV_ERASE");
        SetText("BtnCarDoImportText", "#STR_OZ_DEV_IMPORT");
        SetText("BtnCarCloseText", "#STR_OZ_DEV_CAR_CLOSE");
        m_Preview    = ItemPreviewWidget.Cast(Wgt("Preview"));
        m_ChargeFill = Wgt("ChargeFill");
        m_BtnPower     = ButtonWidget.Cast(Wgt("BtnPower"));
        m_BtnPin       = ButtonWidget.Cast(Wgt("BtnPin"));
        m_BtnPinClear  = ButtonWidget.Cast(Wgt("BtnPinClear"));
        m_BtnAutoLock  = ButtonWidget.Cast(Wgt("BtnAutoLock"));
    }

    override void OnSelected()
    {
        if (m_CarPane)
            m_CarPane.Show(false);

        ClearHintHold();
        Request();
    }

    // Прив'язка ТІЛЬКИ показується. Володіє нею ядро: вона належить гравцеві,
    // а не апарату, переживає втрату КПК і потрібна рації, квестам та ІІ, яким
    // екран ні до чого. Ворота прив'язки -- окреме вікно ядра, і КПК для них
    // не потрібен зовсім.
    private void PaintLink(bool linked)
    {
        if (linked)
            SetHint("LinkText", "#STR_OZ_LINK_DONE");
        else
            SetHint("LinkText", "#STR_OZ_LINK_NONE");
    }

    // Раз на секунду: заряд і радіація змінюються самі, і сторінка мусить це
    // показувати без натискань. Частіше -- марний трафік на кожен тік.
    override void OnRefresh()
    {
        Request();
    }

    private void Request()
    {
        OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "status", "{}");
    }

    // Кнопка живлення. Рішення серверне: клієнт лише каже, чого хоче, і
    // одразу перепитує стан -- малювати «увімкнено» з власної голови означало
    // б показати те, чого може не бути.
    override bool OnPageClick(Widget w, int x, int y)
    {
        if (w == m_BtnCarWriteM)
        {
            CarrierOp("carrier_write", "markers");
            return true;
        }

        if (w == m_BtnCarWriteN)
        {
            CarrierOp("carrier_write", "notes");
            return true;
        }

        if (w == m_BtnCarView)
        {
            CarrierOp("carrier_read", "");
            return true;
        }

        if (w == m_BtnCarClose)
        {
            if (m_CarPane)
                m_CarPane.Show(false);
            return true;
        }

        if (w == m_BtnCarDoImport)
        {
            CarrierOp("carrier_import", "");
            return true;
        }

        if (w == m_BtnCarErase)
        {
            CarrierOp("carrier_erase", "");
            return true;
        }

        if (w && w == m_BtnPower)
        {
            bool want = true;
            if (m_Status)
                want = !m_Status.Powered;

            OZ_PdaFlagOp op = new OZ_PdaFlagOp();
            op.Value = want;

            string json;
            string err;
            if (!JsonFileLoader<OZ_PdaFlagOp>.MakeData(op, json, err, false))
                return true;

            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "power", json);
            return true;
        }

        // Набір коду веде МЕНЮ, а не сторінка: цифри ловить OnKeyPress меню,
        // і другого місця, де живе введений код, бути не повинно.
        if (w && w == m_BtnPin)
        {
            OZ_PdaMenu menu = OZ_PdaMenu.Cast(GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA));
            if (menu)
                menu.BeginPin("set");
            return true;
        }

        if (w && w == m_BtnPinClear)
        {
            OZ_PdaMenu clearMenu = OZ_PdaMenu.Cast(GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA));
            if (clearMenu)
                clearMenu.BeginPin("clear");
            return true;
        }

        if (w && w == m_BtnAutoLock)
        {
            bool wantLock = true;
            if (m_Status)
                wantLock = !m_Status.AutoLock;

            OZ_PdaFlagOp lockOp = new OZ_PdaFlagOp();
            lockOp.Value = wantLock;

            string lockJson;
            string lockErr;
            if (!JsonFileLoader<OZ_PdaFlagOp>.MakeData(lockOp, lockJson, lockErr, false))
                return true;

            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "autolock", lockJson);
            return true;
        }

        return false;
    }

    private void CarrierOp(string op, string kind)
    {
        string json = "{}";
        if (kind != "")
        {
            OZ_CarrierWriteOp w = new OZ_CarrierWriteOp();
            w.Kind = kind;

            string err;
            if (!JsonFileLoader<OZ_CarrierWriteOp>.MakeData(w, json, err, false))
                return;
        }

        OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, op, json);
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        // Відповідь на живлення сама по собі нічого не несе: перепитуємо стан
        // і малюємо його, а причину відмови показуємо там, де вона видима.
        if (op == "carrier_read")
        {
            if (!ok)
            {
                SetHintSticky("CarrierText", "#" + error);
                return;
            }

            ShowCarrierPreview(json);
            return;
        }

        if (op == "carrier_import")
        {
            if (m_CarPane)
                m_CarPane.Show(false);

            if (!ok)
            {
                SetHintSticky("CarrierText", "#" + error);
            }
            else
            {
                // Частковий iмпорт -- НЕ "Done.": скiльки взято проти
                // скiльки лежало, i рiзниця досi на чипi.
                OZ_CarrierTaken t;
                string terr;
                if (JsonFileLoader<OZ_CarrierTaken>.LoadData(json, t, terr) && t && t.Taken < t.Total)
                    SetHintSticky("CarrierText", "#STR_OZ_DEV_CAR_PART  " + t.Taken.ToString() + "/" + t.Total.ToString());
                else
                    SetHintSticky("CarrierText", "#STR_OZ_DEV_CARRIER_DONE");
            }
            Request();
            return;
        }

        if (op == "carrier_write" || op == "carrier_erase")
        {
            if (ok)
                SetHintSticky("CarrierText", "#STR_OZ_DEV_CARRIER_DONE");
            else
                SetHintSticky("CarrierText", "#" + error);
            Request();
            return;
        }

        if (op == "power")
        {
            if (!ok)
                SetHintSticky("ChargeLabel", "#" + error);
            Request();
            return;
        }

        // Пін і автоблокування самі нічого не несуть: перепитуємо стан.
        // Причину відмови по піну малює екран коду, він же її й ловить.
        if (op == "setpin" || op == "autolock")
        {
            if (!ok && op == "autolock")
                SetHintSticky("AutoLockText", "#" + error);
            Request();
            return;
        }

        if (op != "status")
            return;

        if (!ok)
        {
            SetText("TitleName", "#" + error);
            SetText("TitleProfile", "");
            return;
        }

        string err;
        OZ_PdaDeviceStatus st;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.LoadData(json, st, err))
        {
            OZ_Log.Error("device status unreadable: " + err);
            return;
        }

        m_Status = st;
        Paint();
    }

    OZ_PdaDeviceStatus Status()
    {
        return m_Status;
    }

    private void Paint()
    {
        bool hasCar   = m_Status && m_Status.CarrierClass != "";
        bool carWrit  = m_Status && m_Status.CarrierWritten;
        bool carCanW  = m_Status && m_Status.CarrierWritable;

        if (m_BtnCarWriteM)
            m_BtnCarWriteM.Show(hasCar && carCanW);
        if (m_BtnCarWriteN)
            m_BtnCarWriteN.Show(hasCar && carCanW);
        if (m_BtnCarView)
            m_BtnCarView.Show(hasCar && carWrit);
        if (m_CarPane && !(hasCar && carWrit))
            m_CarPane.Show(false);
        if (m_BtnCarErase)
            m_BtnCarErase.Show(hasCar && carWrit && carCanW);

        OZ_PdaDeviceStatus st = m_Status;

        SetText("TitleName", st.DisplayName);

        string prof = st.ProfileId;
        prof += " / " + st.ClassName;
        SetText("TitleProfile", prof);

        // Пристрій у рюкзаку -- це робочий стан, а не помилка, і мовчати про
        // нього не можна: гравець мусить розуміти, чому екран живий, хоч у
        // руках порожньо.
        if (st.InHands)
            SetText("TitlePlace", "");
        else
            SetText("TitlePlace", "#STR_OZ_DEV_STOWED");

        // Показуємо СПРАВЖНІЙ предмет, а не абстрактний екземпляр класу:
        // SetItem бере сутність, і завдяки цьому у прев'ю видно вставлені
        // модулі й батарею, а не порожній корпус.
        if (m_Preview)
        {
            EntityAI dev = OZ_PdaClient.Device(st);
            if (dev)
            {
                m_Preview.SetItem(dev);
                // SetView ОБОВ'ЯЗКОВИЙ: без нього прев'ю не малює нічого.
                // Індекс беремо в самого предмета, як це робить ванільна
                // сітка інвентаря (inventorygrid.c:215).
                m_Preview.SetView(dev.GetViewIndex());
                // Легкий поворот: пристрій анфас читається як плоска пляма.
                m_Preview.SetModelOrientation(Vector(0, 12, 0));
                m_Preview.Show(true);
            }
            else
            {
                // Сутність клієнту ще не приїхала. Порожній віджет краще за
                // чужу модель, що лишилась із минулого разу.
                m_Preview.SetItem(null);
                m_Preview.Show(false);
            }
        }

        PaintCharge(st);
        PaintBays(st);
        PaintCarrier(st);
        PaintSession(st);
        PaintLock(st);
        PaintRadiation(st);
    }

    private void PaintCharge(OZ_PdaDeviceStatus st)
    {
        string label;
        if (!st.Powered)
            label = "#STR_OZ_DEV_OFF";
        else
        {
            int pct = Math.Round(st.Charge01 * 100);
            label = "#STR_OZ_DEV_POWER";
            label += "  " + pct.ToString() + "%";
        }
        SetHint("ChargeLabel", label);

        // Напис на кнопці -- це те, що станеться після натискання, а не те,
        // що є зараз. Без батареї вмикати нічого, і кнопка про це й пише.
        TextWidget pt = Text("PowerText");
        if (pt)
        {
            if (!st.HasBattery)
                pt.SetText("#STR_OZ_DEV_POWER_NO_BATT");
            else if (st.Powered)
                pt.SetText("#STR_OZ_DEV_POWER_OFF");
            else
                pt.SetText("#STR_OZ_DEV_POWER_ON");
        }

        if (m_ChargeFill)
        {
            // Ширина смуги -- у тих самих одиницях розмітки, що й у layout.
            float w = 677 * Math.Clamp(st.Charge01, 0, 1);
            m_ChargeFill.SetSize(w, 10);
        }
    }

    private void PaintBays(OZ_PdaDeviceStatus st)
    {
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
        {
            string name = "BayText" + i.ToString();
            TextWidget w = Text(name);
            if (!w)
                continue;

            OZ_BayInfo bay = null;
            for (int b = 0; b < st.Bays.Count(); b++)
            {
                if (st.Bays[b].Index == i)
                {
                    bay = st.Bays[b];
                    break;
                }
            }

            if (!bay || !bay.Visible)
            {
                // Відсіку в цієї моделі немає -- рядок ховаємо зовсім, а не
                // пишемо «недоступно»: місця немає, і його не буде.
                w.Show(false);
                continue;
            }

            w.Show(true);

            string line = "[" + (i + 1).ToString() + "]  ";
            if (bay.ClassName == "")
                line += "#STR_OZ_DEV_BAY_EMPTY";
            else if (bay.Display != "")
                line += bay.Display;
            else
                line += bay.ClassName;

            w.SetText(line);
        }
    }

    // Зібрати текст прев'ю з того, що чип чесно віддав. Знаємо СВОЇ два
    // роди; чужий показуємо словом роду -- його вміст знає лише його
    // сторінка, а гравцеві досить бачити, що чип не порожній.
    private void ShowCarrierPreview(string json)
    {
        OZ_CarrierView v;
        string err;
        if (!JsonFileLoader<OZ_CarrierView>.LoadData(json, v, err) || !v)
            return;

        string head = "";
        string body = "";

        if (v.Kind == "markers")
        {
            OZ_MarkerList ml;
            if (JsonFileLoader<OZ_MarkerList>.LoadData(v.Payload, ml, err) && ml && ml.Items)
            {
                head = "#STR_OZ_DEV_CAR_MARKS  " + ml.Items.Count().ToString();
                for (int i = 0; i < ml.Items.Count(); i++)
                {
                    OZ_MapMarker m = ml.Items[i];
                    vector at = m.Pos.ToVector();
                    int px = Math.Round(at[0]);
                    int pz = Math.Round(at[2]);

                    body += m.Name + "  @ " + px.ToString() + " " + pz.ToString();
                    if (m.Desc != "")
                        body += "  -- " + OneLine(m.Desc);
                    body += "\n";
                }
            }
        }
        else if (v.Kind == "notes")
        {
            OZ_NoteBook nb;
            if (JsonFileLoader<OZ_NoteBook>.LoadData(v.Payload, nb, err) && nb && nb.Notes)
            {
                head = "#STR_OZ_DEV_CAR_NOTES  " + nb.Notes.Count().ToString();
                for (int k = 0; k < nb.Notes.Count(); k++)
                {
                    OZ_Note n = nb.Notes[k];
                    body += n.Title;
                    if (n.Body != "")
                        body += "  -- " + OneLine(n.Body);
                    body += "\n";
                }
            }
        }

        if (head == "")
        {
            head = v.Kind;
            body = "#STR_OZ_DEV_CARRIER_UNKNOWN";
        }

        SetText("CarPaneHead", head);

        TextWidget tw = TextWidget.Cast(Wgt("CarBody"));
        if (tw)
            tw.SetText(body);

        if (m_CarPane)
            m_CarPane.Show(true);
    }

    // Один рядок прев'ю на запис: переноси -- в пробіли, хвіст -- геть.
    private string OneLine(string text)
    {
        string t = text;
        t.Replace("\n", " ");

        string cut = OZ_Text.Clip(t, 96);
        if (cut.Length() < t.Length())
            cut += "...";
        return cut;
    }

    private void PaintCarrier(OZ_PdaDeviceStatus st)
    {
        string line = "#STR_OZ_DEV_CARRIER";
        line += "  ";

        if (st.CarrierClass == "")
        {
            line += "#STR_OZ_DEV_BAY_EMPTY";
        }
        else if (!st.CarrierWritten)
        {
            line += "#STR_OZ_DEV_CARRIER_BLANK";
        }
        else if (st.CarrierKind != "")
        {
            // Свої роди -- людським словом; чужий (iнший мод зi своєю
            // сторiнкою) -- як є: його слово знає лише його сторiнка.
            if (st.CarrierKind == "markers")
                line += "#STR_OZ_DEV_CAR_MARKS";
            else if (st.CarrierKind == "notes")
                line += "#STR_OZ_DEV_CAR_NOTES";
            else
                line += st.CarrierKind;

            if (st.CarrierCount >= 0)
                line += "  " + st.CarrierCount.ToString();
        }
        else
        {
            line += "#STR_OZ_DEV_CARRIER_UNKNOWN";
        }

        // SetHint, НЕ SetText: результат опа над носiєм липкий, а цей рядок
        // перемальовується щосекунди -- прямий запис з'їдав його до того, як
        // око встигало прочитати. Рiвно та хвороба, яку лiкує HINT_HOLD_MS.
        SetHint("CarrierText", line);
    }

    private void PaintSession(OZ_PdaDeviceStatus st)
    {
        if (st.Online)
        {
            SetText("SessionText", "#STR_OZ_DEV_ONLINE");
            SetText("SnapshotText", "");
            return;
        }

        // Офлайн -- це НЕ поломка, і сказати про це треба так, щоб гравець
        // зрозумів: пристрій живий, але показує світ станом на певну мить.
        SetText("SessionText", "#STR_OZ_DEV_OFFLINE");

        if (st.SnapshotAt != "")
        {
            string s = "#STR_OZ_DEV_SNAPSHOT";
            s += "  " + st.SnapshotAt;
            SetText("SnapshotText", s);
        }
        else
            SetText("SnapshotText", "#STR_OZ_DEV_NO_SNAPSHOT");
    }

    private void PaintLock(OZ_PdaDeviceStatus st)
    {
        if (!st.HasPin)
            SetText("PinText", "#STR_OZ_DEV_NO_PIN");
        else if (st.Unlocked)
            SetText("PinText", "#STR_OZ_DEV_PIN_OPEN");
        else
            SetText("PinText", "#STR_OZ_DEV_PIN_SET");

        string al = "#STR_OZ_DEV_AUTOLOCK";
        al += "  ";
        if (!st.AutoLock)
            al += "#STR_OZ_OFF";
        else
        {
            al += "#STR_OZ_ON";
            if (st.LockAfterMinutes > 0)
            {
                int m = Math.Round(st.LockAfterMinutes);
                al += " (" + m.ToString() + " min)";
            }
        }
        SetHint("AutoLockText", al);

        PaintLink(st.DiscordLinked);

        // Написи на кнопках -- це те, що станеться після натискання.
        if (st.HasPin)
            SetText("BtnPinText", "#STR_OZ_PIN_CHANGE_BTN");
        else
            SetText("BtnPinText", "#STR_OZ_PIN_SET_BTN");

        SetText("BtnPinClearText", "#STR_OZ_PIN_CLEAR_BTN");

        // Знімати код нема з чого, поки його немає: кнопка не сіріє, а зникає.
        if (m_BtnPinClear)
            m_BtnPinClear.Show(st.HasPin);

        // Так само з автоблокуванням: якщо сервер заборонив його вимикати,
        // кнопки просто немає -- замість кнопки, яка завжди відмовляє.
        if (m_BtnAutoLock)
            m_BtnAutoLock.Show(!st.ForceAutoLock);

        if (st.AutoLock)
            SetText("BtnAutoLockText", "#STR_OZ_AUTOLOCK_OFF_BTN");
        else
            SetText("BtnAutoLockText", "#STR_OZ_AUTOLOCK_ON_BTN");
    }

    private void PaintRadiation(OZ_PdaDeviceStatus st)
    {
        TextWidget w = Text("RadText");
        if (!w)
            return;

        bool anyModule = false;
        for (int b = 0; b < st.Bays.Count(); b++)
        {
            string k = st.Bays[b].Kind;
            if (k == OZ_PdaConst.MOD_RADIOMETER || k == OZ_PdaConst.MOD_DOSIMETER)
            {
                anyModule = true;
                break;
            }
        }

        if (!anyModule)
        {
            w.Show(false);
            return;
        }

        w.Show(true);

        if (!st.HasRadiationProvider)
        {
            // Приладу є, даних немає. Нуль тут був би брехнею: нуль означає
            // «чисто».
            w.SetText("#STR_OZ_RAD_NO_PROVIDER");
            return;
        }

        string line = "";

        if (st.AmbientUSvH >= 0)
        {
            line += "#STR_OZ_RAD_AMBIENT";
            line += " " + st.AmbientUSvH.ToString() + " uSv/h";
        }

        if (st.DoseUSv >= 0)
        {
            if (line != "")
                line += "     ";
            line += "#STR_OZ_RAD_DOSE";
            line += " " + st.DoseUSv.ToString() + " uSv";
        }

        if (line == "")
            line = "#STR_OZ_RAD_NO_DATA";

        w.SetText(line);
    }
}
