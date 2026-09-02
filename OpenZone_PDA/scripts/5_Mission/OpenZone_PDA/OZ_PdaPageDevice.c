// Сторінка «Пристрій»: що це за КПК, чим живиться, що в нього вставлено.
//
// Усе, що вона показує, приходить одним запитом device/status. Клієнт нічого
// не обчислює сам -- інакше з'явилась би друга правда про стан пристрою.

class OZ_PdaPageDevice : OZ_PdaPage
{
    private int m_Beat = 0;
    private ItemPreviewWidget m_Preview;
    private Widget m_ChargeFill;
    private ButtonWidget m_BtnPower;
    private ButtonWidget m_BtnPin;
    private ButtonWidget m_BtnPinClear;
    private ButtonWidget m_BtnAutoLock;
    private ButtonWidget m_BtnLock;
    private ButtonWidget m_BtnInit;
    private ButtonWidget m_BtnFactoryDev;
    private ButtonWidget m_BtnLogout;

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
    private ButtonWidget m_BtnCarTake;
    private ButtonWidget m_BtnCarDel;
    private TextListboxWidget m_CarList;

    // Розібране прев'ю чипа: рядок списку -> (секція, місце в ній).
    private ref OZ_MarkerList  m_CarMarks;
    private ref OZ_NoteBook    m_CarNotes;
    private ref array<string>  m_CarRowKind = new array<string>();
    private ref array<int>     m_CarRowIndex = new array<int>();
    private int m_CarSelRow = -1;
    private ButtonWidget m_BtnHudEdit;

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
        m_BtnCarTake     = ButtonWidget.Cast(Wgt("BtnCarTake"));
        m_BtnCarDel      = ButtonWidget.Cast(Wgt("BtnCarDel"));
        m_CarList        = TextListboxWidget.Cast(Wgt("CarList"));


        SetText("BtnCarWriteMText", "#STR_OZ_DEV_WRITE_MARKS");
        SetText("BtnCarWriteNText", "#STR_OZ_DEV_WRITE_NOTES");
        SetText("BtnCarViewText", "#STR_OZ_DEV_CAR_VIEW");
        SetText("BtnCarEraseText", "#STR_OZ_DEV_ERASE");
        SetText("BtnCarDoImportText", "#STR_OZ_DEV_IMPORT");
        SetText("BtnCarCloseText", "#STR_OZ_DEV_CAR_CLOSE");
        SetText("BtnCarTakeText", "#STR_OZ_DEV_CAR_TAKE");
        SetText("BtnCarDelText", "#STR_OZ_DEV_CAR_DEL");
        m_BtnHudEdit = ButtonWidget.Cast(Wgt("BtnHudEdit"));
        SetText("BtnHudEditText", "#STR_OZ_DEV_HUD_EDIT");
        m_Preview    = ItemPreviewWidget.Cast(Wgt("Preview"));
        m_ChargeFill = Wgt("ChargeFill");
        m_BtnPower     = ButtonWidget.Cast(Wgt("BtnPower"));
        m_BtnPin       = ButtonWidget.Cast(Wgt("BtnPin"));
        m_BtnPinClear  = ButtonWidget.Cast(Wgt("BtnPinClear"));
        m_BtnAutoLock  = ButtonWidget.Cast(Wgt("BtnAutoLock"));
        m_BtnLock      = ButtonWidget.Cast(Wgt("BtnLock"));
        SetText("BtnLockText", "#STR_OZ_DEV_LOCK_NOW");
        m_BtnInit      = ButtonWidget.Cast(Wgt("BtnInit"));
        SetText("BtnInitText", "#STR_OZ_DEV_INIT");
        m_BtnFactoryDev = ButtonWidget.Cast(Wgt("BtnFactoryDev"));
        SetText("BtnFactoryDevText", "#STR_OZ_FACTORY_RESET");
        m_BtnLogout = ButtonWidget.Cast(Wgt("BtnLogout"));
        SetText("BtnLogoutText", "#STR_OZ_DEV_LOGOUT_OTHERS");
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
        // Страховка раз на 5 секунд: живі зміни приходять пушами одразу,
        // щосекундний перепит лише дублював їх (аудит 2026-08-30).
        m_Beat++;
        if (m_Beat % 5 != 0)
            return;
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

        if (w == m_BtnHudEdit)
        {
            // Редактор -- ОКРЕМЕ меню поверх світу: КПК закривається, і
            // гравець бачить екран, який розкладає, а не пристрій.
            // Панелі мусять ІСНУВАТИ до відкриття: якщо HUD ще не був
            // видимим цієї сесії, реєстр порожній і тягати нема чого.
            OZ_PdaHud.EnsurePanes();
            OZ_PdaMenuGate.Close();
            GetGame().GetUIManager().EnterScriptedMenu(OZ_PdaConst.MENU_PDA_HUD, null);
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

        if (w == m_BtnCarTake)
        {
            SendCarrierItem("carrier_take");
            return true;
        }

        if (w == m_BtnCarDel)
        {
            SendCarrierItem("carrier_del");
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

        if (w && w == m_BtnLock)
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "lock", "{}");
            return true;
        }

        if (w && w == m_BtnInit)
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "initiate", "{}");
            return true;
        }

        if (w && w == m_BtnFactoryDev)
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "factory_reset", "{}");
            return true;
        }

        if (w && w == m_BtnLogout)
        {
            OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, "logout_others", "{}");
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

        if (op == "carrier_take")
        {
            if (ok)
                SetHintSticky("CarPaneHead", "#STR_OZ_DEV_CAR_TAKEN");
            else
                SetHintSticky("CarPaneHead", "#" + error);
            return;
        }

        if (op == "carrier_del")
        {
            if (!ok)
            {
                SetHintSticky("CarPaneHead", "#" + error);
                return;
            }

            // Перечитуємо чип: список без стертого рядка малює сервер,
            // а не наша здогадка про нього.
            CarrierOp("carrier_read", "");
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

        if (op == "lock")
        {
            if (!ok)
                SetHintSticky("PinText", "#" + error);
            Request();
            return;
        }

        if (op == "initiate")
        {
            if (!ok)
            {
                SetHintSticky("SessionText", "#" + error);
                return;
            }

            // Стрічка вкладок будується ОДИН раз за відкриття, і в нової
            // сесії їх більше: закриваємо меню, наступне відкриття збере
            // повний набір.
            OZ_PdaMenu menu = OZ_PdaMenu.Cast(GetGame().GetUIManager().FindMenu(OZ_PdaConst.MENU_PDA));
            if (menu)
                menu.Close();
            return;
        }

        if (op == "factory_reset")
        {
            if (!ok)
            {
                SetHintSticky("SessionText", "#" + error);
                return;
            }
            Request();
            return;
        }

        if (op == "logout_others")
        {
            if (!ok)
                SetHintSticky("SessionText", "#" + error);
            else
                SetHintSticky("SessionText", "#STR_OZ_DEV_LOGGED_OUT");
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
            // ЛИПКО Й У ВИДИМИЙ ВІДЖЕТ -- так само, як роблять решта
            // сімнадцять місць на цій сторінці.
            //
            // Причина відмови писалась у TitleName, тобто в панель прев'ю
            // предмета -- а вона на цьому екрані ПРИХОВАНА (PreviewPane
            // show=0). Тобто спільний шлях відмови, крізь який проходить
            // кожне «прилад вимкнено», «прилад замкнено», «немає доступу»,
            // не показував нічого взагалі. А ще й не липко: перший же
            // успішний status затер би його за частку секунди.
            SetHintSticky("SessionText", "#" + error);
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

    // Прев'ю чипа: обидві секції одним СПИСКОМ, кожен рядок клікабельний --
    // вибір показує повний запис праворуч, а кнопки TAKE/DEL працюють саме
    // з вибраним. Кольором кажемо «натисни»: то самий акцент, що в чаті.
    private void ShowCarrierPreview(string json)
    {
        OZ_CarrierView v;
        string err;
        if (!JsonFileLoader<OZ_CarrierView>.LoadData(json, v, err) || !v)
            return;

        m_CarMarks = null;
        m_CarNotes = null;
        m_CarRowKind.Clear();
        m_CarRowIndex.Clear();
        m_CarSelRow = -1;

        // Секції приїжджають уже об'єктами -- див. коментар в OZ_CarrierView.
        if (v.Marks && v.Marks.Items)
            m_CarMarks = v.Marks;

        if (v.Notes && v.Notes.Notes)
            m_CarNotes = v.Notes;

        // Шапка -- місткість: «скільки з скількох» на кожну секцію. Стеля
        // 0 означає безліміт, і тоді число стоїть саме.
        int markCnt = 0;
        if (m_CarMarks)
            markCnt = m_CarMarks.Items.Count();
        int noteCnt = 0;
        if (m_CarNotes)
            noteCnt = m_CarNotes.Notes.Count();

        string head = "#STR_OZ_DEV_CAR_MARKS " + markCnt.ToString();
        if (v.MaxRecords > 0)
            head += "/" + v.MaxRecords.ToString();
        head += "   #STR_OZ_DEV_CAR_NOTES " + noteCnt.ToString();
        if (v.MaxRecords > 0)
            head += "/" + v.MaxRecords.ToString();
        SetText("CarPaneHead", head);

        if (m_CarList)
        {
            m_CarList.ClearItems();

            if (m_CarMarks)
            {
                for (int i = 0; i < m_CarMarks.Items.Count(); i++)
                {
                    OZ_MapMarker m = m_CarMarks.Items[i];
                    // Чужий чип -- чужий JSON: масив може нести null-елементи.
                    if (!m)
                        continue;

                    // Лише назва: координати живуть у прев'ю праворуч, а
                    // рядок списку мусить читатись одним поглядом.
                    int row = m_CarList.AddItem("[M] " + m.Name, NULL, 0);
                    m_CarList.SetItemColor(row, 0, ARGB(255, 255, 122, 26));
                    m_CarRowKind.Insert("mark");
                    m_CarRowIndex.Insert(i);
                }
            }

            if (m_CarNotes)
            {
                for (int k = 0; k < m_CarNotes.Notes.Count(); k++)
                {
                    OZ_Note n = m_CarNotes.Notes[k];
                    if (!n)
                        continue;

                    int nrow = m_CarList.AddItem("[N] " + n.Title, NULL, 0);
                    m_CarList.SetItemColor(nrow, 0, ARGB(255, 255, 122, 26));
                    m_CarRowKind.Insert("note");
                    m_CarRowIndex.Insert(k);
                }
            }
        }

        SetText("CarItemHead", "");
        SetCarBody("#STR_OZ_DEV_CAR_TAP");

        if (m_CarPane)
            m_CarPane.Show(true);
    }

    // Тіло прев'ю з ЧЕСНОЮ висотою вмісту: скрол їздить рівно по тексту,
    // а не по запасних восьмистах пікселях. Висота -- оцінка по рядках
    // (перенос ~38 символів на 307px тим шрифтом), і її вистачає, бо
    // ціль -- межа прокрутки, а не типографіка.
    private void SetCarBody(string text)
    {
        TextWidget tw = TextWidget.Cast(Wgt("CarBody"));
        if (!tw)
            return;

        tw.SetText(text);

        int lines = 0;
        int seg = 0;
        for (int ci = 0; ci < text.Length(); ci++)
        {
            if (text.Substring(ci, 1) == "\n")
            {
                lines += 1 + seg / 66;
                seg = 0;
            }
            else
            {
                seg++;
            }
        }
        lines += 1 + seg / 66;

        int h = lines * 18 + 6;
        if (h < 108)
            h = 108;
        tw.SetSize(580, h);
    }

    // Клік по рядку списку: повний запис у праву панель.
    override bool OnPageItemSelected(Widget w, int row)
    {
        if (!m_CarList || w != m_CarList)
            return false;

        if (row < 0 || row >= m_CarRowKind.Count())
            return true;

        m_CarSelRow = row;

        string head = "";
        string body = "";
        if (m_CarRowKind[row] == "mark" && m_CarMarks)
        {
            OZ_MapMarker m = m_CarMarks.Items[m_CarRowIndex[row]];
            if (m)
            {
                vector at = m.Pos.ToVector();
                head = m.Name;
                body = "@ " + Math.Round(at[0]).ToString() + " " + Math.Round(at[2]).ToString();
                if (m.Desc != "")
                    body += "\n\n" + m.Desc;
            }
        }
        else if (m_CarRowKind[row] == "note" && m_CarNotes)
        {
            OZ_Note n = m_CarNotes.Notes[m_CarRowIndex[row]];
            if (n)
            {
                head = n.Title;
                body = n.Body;
            }
        }

        SetText("CarItemHead", head);
        SetCarBody(body);

        return true;
    }

    // TAKE або DEL для вибраного рядка. Без вибору чесно кажемо чому ні.
    private void SendCarrierItem(string op)
    {
        if (m_CarSelRow < 0 || m_CarSelRow >= m_CarRowKind.Count())
        {
            SetHintSticky("CarPaneHead", "#STR_OZ_DEV_CAR_TAP");
            return;
        }

        OZ_CarrierItemRef r = new OZ_CarrierItemRef();
        r.Kind  = m_CarRowKind[m_CarSelRow];
        r.Index = m_CarRowIndex[m_CarSelRow];

        string json;
        string err;
        if (!JsonFileLoader<OZ_CarrierItemRef>.MakeData(r, json, err, false))
            return;

        OZ_Rpc.Request(OZ_PdaConst.PAGE_DEVICE, op, json);
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
        else
        {
            // Обидві секції з місткістю класу: «скільки з скількох».
            // Стеля 0 -- безліміт, тоді число стоїть саме.
            string parts = "";

            if (st.CarrierMarks >= 0)
            {
                parts += "#STR_OZ_DEV_CAR_MARKS " + st.CarrierMarks.ToString();
                if (st.CarrierMaxRecords > 0)
                    parts += "/" + st.CarrierMaxRecords.ToString();
            }

            if (st.CarrierNotes >= 0)
            {
                if (parts != "")
                    parts += "   ";
                parts += "#STR_OZ_DEV_CAR_NOTES " + st.CarrierNotes.ToString();
                if (st.CarrierMaxRecords > 0)
                    parts += "/" + st.CarrierMaxRecords.ToString();
            }

            if (parts == "")
                parts = "#STR_OZ_DEV_CARRIER_UNKNOWN";

            line += parts;
        }

        // SetHint, НЕ SetText: результат опа над носiєм липкий, а цей рядок
        // перемальовується щосекунди -- прямий запис з'їдав його до того, як
        // око встигало прочитати. Рiвно та хвороба, яку лiкує HINT_HOLD_MS.
        SetHint("CarrierText", line);
    }

    private void PaintSession(OZ_PdaDeviceStatus st)
    {
        if (!st.Owned)
        {
            // Нічий: чесно кажемо, що пристрій чекає ініціації.
            SetText("SessionText", "#STR_OZ_DEV_UNOWNED");
            SetText("SnapshotText", "");
            return;
        }

        if (st.Online)
        {
            string on = "#STR_OZ_DEV_ONLINE";
            if (st.OwnerName != "")
                on += "  " + st.OwnerName;
            SetText("SessionText", on);
            SetText("SnapshotText", "");
            return;
        }

        // Офлайн -- це НЕ поломка, і сказати про це треба так, щоб гравець
        // зрозумів: пристрій живий, але показує світ станом на певну мить.
        // Ім'я власника -- тут же, як і в онлайна: чия це капсула, видно
        // з першого рядка, а не з розбору дайджеста.
        string off = "#STR_OZ_DEV_OFFLINE";
        if (st.OwnerName != "")
            off += "  " + st.OwnerName;
        SetText("SessionText", off);

        if (st.SnapshotAt != "")
        {
            string s = "#STR_OZ_DEV_SNAPSHOT";
            s += "  " + OZ_LocalTime.Stamp(st.SnapshotAt);

            // Капсула часу: що встиг запам'ятати пристрій, поки був живим.
            if (st.Snapshot != "")
            {
                OZ_PdaSnapshot snap;
                string serr;
                if (JsonFileLoader<OZ_PdaSnapshot>.LoadData(st.Snapshot, snap, serr) && snap)
                {
                    s += "\n#STR_OZ_DEV_SNAP_OWNER " + snap.Owner;
                    // ОБИДВІ осі в дужках, через кому: «(Сталкер, Долг)».
                    // Показати саму лише організацію означало б, що одинак
                    // у капсулі виглядає безіменним, хоча про нього відомо
                    // рівно стільки ж, скільки про борговця.
                    string who = snap.Base;
                    if (snap.Org != "")
                    {
                        if (who != "")
                            who += ", ";
                        who += snap.Org;
                    }
                    if (who != "")
                        s += " (" + who + ")";
                    if (snap.Contacts && snap.Contacts.Count() > 0)
                    {
                        s += "\n#STR_OZ_DEV_SNAP_CONTACTS " + snap.Contacts.Count().ToString() + ": ";
                        for (int ci = 0; ci < snap.Contacts.Count(); ci++)
                        {
                            if (ci > 0)
                                s += ", ";
                            s += snap.Contacts[ci];
                        }
                    }
                }
            }

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

        // Ручний замок має сенс лише тому, хто зараз усередині: пін є і
        // пристрій розімкнено.
        if (m_BtnLock)
            m_BtnLock.Show(st.HasPin && st.Unlocked);

        // ІНІЦІАЦІЯ -- лише нічийному пристрою; скидання -- будь-якому
        // відімкненому (свій теж можна обнулити, рішення власника).
        // Ініціацію веде ЕКРАН меню (InitPanel): нічийний пристрій до
        // сторінок узагалі не пускає. Кнопка тут лишилась би мертвою.
        if (m_BtnInit)
            m_BtnInit.Show(false);
        if (m_BtnFactoryDev)
            m_BtnFactoryDev.Show(st.Powered && st.Unlocked);
        if (m_BtnLogout)
            m_BtnLogout.Show(st.Powered && st.SessionMine);

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
