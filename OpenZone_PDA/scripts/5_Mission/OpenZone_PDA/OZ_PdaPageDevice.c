// Сторінка «Пристрій»: що це за КПК, чим живиться, що в нього вставлено.
//
// Усе, що вона показує, приходить одним запитом device/status. Клієнт нічого
// не обчислює сам -- інакше з'явилась би друга правда про стан пристрою.

class OZ_PdaPageDevice : OZ_PdaPage
{
    private ItemPreviewWidget m_Preview;
    private Widget m_ChargeFill;
    private ButtonWidget m_BtnPower;
    private ref OZ_PdaDeviceStatus m_Status;

    override string LayoutPath()
    {
        return "OpenZone_PDA/gui/layouts/oz_pda_page_device.layout";
    }

    override void OnBuilt()
    {
        m_Preview    = ItemPreviewWidget.Cast(Wgt("Preview"));
        m_ChargeFill = Wgt("ChargeFill");
        m_BtnPower   = ButtonWidget.Cast(Wgt("BtnPower"));
    }

    override void OnSelected()
    {
        Request();
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
    override bool OnPageClick(Widget w)
    {
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

        return false;
    }

    override void OnResponse(string op, bool ok, string json, string error)
    {
        // Відповідь на живлення сама по собі нічого не несе: перепитуємо стан
        // і малюємо його, а причину відмови показуємо там, де вона видима.
        if (op == "power")
        {
            if (!ok)
                SetText("ChargeLabel", "#" + error);
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
        SetText("ChargeLabel", label);

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

    private void PaintCarrier(OZ_PdaDeviceStatus st)
    {
        string line = "#STR_OZ_DEV_CARRIER";
        line += "  ";

        if (st.CarrierClass == "")
            line += "#STR_OZ_DEV_BAY_EMPTY";
        else if (!st.CarrierWritten)
            line += "#STR_OZ_DEV_CARRIER_BLANK";
        else if (st.CarrierKind != "")
            line += st.CarrierKind;
        else
            line += "#STR_OZ_DEV_CARRIER_UNKNOWN";

        SetText("CarrierText", line);
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
        SetText("AutoLockText", al);
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
