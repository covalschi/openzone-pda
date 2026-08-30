// Серверна половина КПК: реєструє свої сторінки, читає профілі пристроїв і
// підміняє ядерну заглушку доступу справжньою перевіркою.

class OZ_PdaHandlerDevice : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "status")
            return Status(sender, ok, error);

        if (op == "unlock")
            return Unlock(json, sender, ok, error);

        if (op == "logout_others")
            return LogoutOthers(sender, ok, error);

        if (op == "initiate")
            return Initiate(sender, ok, error);

        if (op == "lock")
            return Lock(sender, ok, error);

        if (op == "factory_reset")
            return FactoryReset(sender, ok, error);

        if (op == "autolock")
            return AutoLock(json, sender, ok, error);

        if (op == "power")
            return Power(json, sender, ok, error);

        if (op == "setpin")
            return SetPin(json, sender, ok, error);

        if (op == "crack")
            return Crack(sender, ok, error);

        if (op == "sealed")
            return Sealed(sender, ok, error);

        if (op == "carrier_write")
            return CarrierWrite(json, sender, ok, error);

        if (op == "carrier_read")
            return CarrierRead(sender, ok, error);

        if (op == "carrier_import" || op == "carrier_take")
        {
            // КАПСУЛА не приймає нічого нового -- ні в пам'ять пристрою,
            // ні в акаунт власника: імпорт із чипа зачинено в обидва боки.
            // Сам чип лишається живим носієм: запис, читання і чистка чипа
            // працюють -- це фізика гнізда, не пам'ять пристрою.
            if (OZ_PdaCapsule.IsFrozen(OZ_PdaLookup.HeldBy(sender)))
            {
                error = "STR_OZ_ERR_FROZEN";
                return "";
            }
        }

        if (op == "carrier_import")
            return CarrierImport(sender, ok, error);

        if (op == "carrier_take")
            return CarrierTake(json, sender, ok, error);

        if (op == "carrier_del")
            return CarrierDel(json, sender, ok, error);

        if (op == "carrier_erase")
            return CarrierErase(sender, ok, error);

        return "";
    }

    // ------------------------------------------------------------ носій
    //
    // Чип -- фізична річ для фізичного обміну: записав мітки, віддав у руки,
    // той вставив і забрав собі. Пейлоад живе НА ПРЕДМЕТІ (CF ModStorage),
    // переживає рестарти і їде з чипом у кишені, у сховку, на трупі.
    //
    // Ворота доступу безкоштовні: ці опи не входять у винятки живлення й
    // замка, тож OZ_PdaAccess вже вимагає ввімкнений і відімкнений пристрій.

    private OZ_DataCarrier_Base CarrierOf(PlayerIdentity sender, out string error)
    {
        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return null;
        }

        OZ_DataCarrier_Base c = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
        if (!c)
        {
            error = "STR_OZ_ERR_NO_CARRIER";
            return null;
        }

        return c;
    }

    private string CarrierWrite(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_CarrierWriteOp opw;
        string err;
        if (!JsonFileLoader<OZ_CarrierWriteOp>.LoadData(json, opw, err) || !opw)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        // Перше справжнє застосування Writable з Hardware.json: чип, який
        // конфіг оголосив лише читаним, не перезаписується ніколи. Клас БЕЗ
        // запису в таблиці -- теж замок: клієнт такому кнопок не малює, і
        // підроблений запит не має права пройти там, де чесний не пройде.
        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (!spec || !spec.Writable)
        {
            error = "STR_OZ_ERR_CARRIER_LOCKED";
            return "";
        }

        // Гейта роду немає: секції незалежні, запис міток не чіпає записок
        // і навпаки. Стирання лишилось окремою дією для чистки ОБОХ.
        if (opw.Kind == "markers")
        {
            OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
            string payload = pda.OZ_MarkersJson();
            if (payload == "")
                payload = "{\"Version\":1,\"Items\":[]}";

            int cnt = 0;
            OZ_MarkerList pl;
            string perr;
            if (JsonFileLoader<OZ_MarkerList>.LoadData(payload, pl, perr) && pl && pl.Items)
                cnt = pl.Items.Count();

            // Місткість класу: на дискету йде стільки, скільки влазить, --
            // ПЕРШІ зі списку, і відповідь чесно каже скільки.
            int wrote = cnt;
            if (spec.MaxMarks > 0 && pl && pl.Items && cnt > spec.MaxMarks)
            {
                pl.Items.Resize(spec.MaxMarks);
                wrote = spec.MaxMarks;

                if (!JsonFileLoader<OZ_MarkerList>.MakeData(pl, payload, perr, false))
                {
                    error = "STR_OZ_ERR_INTERNAL";
                    return "";
                }
            }

            c.OZ_WriteMarks(payload, wrote);

            OZ_CarrierTaken wt = new OZ_CarrierTaken();
            wt.Taken = wrote;
            wt.Total = cnt;

            string wtj;
            if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(wt, wtj, perr, false))
                wtj = "";

            ok = true;
            error = "";
            return wtj;
        }

        if (opw.Kind == "notes")
        {
            // Записки живуть У ПРИСТРОЇ (рішення власника 2026-08-28):
            // книжка вже в руках, міст не потрібен, відповідь синхронна.
            // Місткість класу чипа: на малий носій лягають ПЕРШІ записки,
            // і відповідь чесно каже скільки з скількох.
            OZ_PDA_Base pdaW = OZ_PdaLookup.HeldBy(sender);

            OZ_NoteBook bookW = new OZ_NoteBook();
            if (pdaW.OZ_NotesJson() != "")
            {
                OZ_NoteBook parsedW;
                if (JsonFileLoader<OZ_NoteBook>.LoadData(pdaW.OZ_NotesJson(), parsedW, err) && parsedW && parsedW.Notes)
                    bookW = parsedW;
            }

            int totalW = bookW.Notes.Count();
            int wroteW = totalW;
            if (spec.MaxNotes > 0 && totalW > spec.MaxNotes)
            {
                bookW.Notes.Resize(spec.MaxNotes);
                wroteW = spec.MaxNotes;
            }

            string payloadW;
            if (!JsonFileLoader<OZ_NoteBook>.MakeData(bookW, payloadW, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            c.OZ_WriteNotes(payloadW, wroteW);

            OZ_CarrierTaken wtN = new OZ_CarrierTaken();
            wtN.Taken = wroteW;
            wtN.Total = totalW;

            string wtjN;
            if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(wtN, wtjN, err, false))
                wtjN = "";

            ok = true;
            error = "";
            return wtjN;
        }

        error = "STR_OZ_ERR_INTERNAL";
        return "";
    }

    private string CarrierRead(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        if (!c.OZ_IsWritten())
        {
            error = "STR_OZ_ERR_CARRIER_BLANK";
            return "";
        }

        // Секції РОЗБИРАЄМО ТУТ і віддаємо об'єктами: рядок-значення в JSON
        // клієнт зрізав би на 1023 байтах (зміряно). Що на чипі -- те й
        // видно, і саме тому крадений КПК з чужим чипом читає чужі мітки:
        // така ціна фізичного носія, і вона навмисна.
        OZ_CarrierView v = new OZ_CarrierView();

        string serr;
        if (c.OZ_Marks() != "")
        {
            OZ_MarkerList vm;
            if (JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Marks(), vm, serr) && vm && vm.Items)
                v.Marks = vm;
        }
        if (c.OZ_Notes() != "")
        {
            OZ_NoteBook vn;
            if (JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Notes(), vn, serr) && vn && vn.Notes)
                v.Notes = vn;
        }

        OZ_CarrierSpec vspec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (vspec)
        {
            v.MaxMarks = vspec.MaxMarks;
            v.MaxNotes = vspec.MaxNotes;
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_CarrierView>.MakeData(v, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string CarrierImport(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        if (!c.OZ_IsWritten())
        {
            error = "STR_OZ_ERR_CARRIER_BLANK";
            return "";
        }

        // Спершу МІТКИ -- вони локальні й лягають одразу; записки їдуть
        // слідом через міст, і їхній результат приїде окремою відповіддю.
        int marksTaken = -1;
        int marksTotal = -1;

        if (c.OZ_Marks() != "")
        {
            OZ_MarkerList incoming;
            string err;
            if (!JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Marks(), incoming, err) || !incoming || !incoming.Items)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
            OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());

            OZ_MarkerList mine = new OZ_MarkerList();
            string raw = pda.OZ_MarkersJson();
            if (raw != "")
            {
                OZ_MarkerList parsed;
                if (JsonFileLoader<OZ_MarkerList>.LoadData(raw, parsed, err) && parsed && parsed.Items)
                    mine = parsed;
            }

            int limit = 0;
            if (prof && prof.Limits)
                limit = prof.Limits.Markers;

            // limit <= 0 -- зіпсований конфіг. Сторінка карти в цьому стані
            // відмовляє СТАВИТИ, тож імпорт поводиться так само, а не читає
            // той самий нуль як «безліміт». Але ЧЕСНА відмова доречна лише
            // коли на чипі самі мітки: змішаний чип мусить донести записки,
            // а мітки тоді просто «0 з N».
            if (limit <= 0 || mine.Items.Count() >= limit)
            {
                if (c.OZ_Notes() == "")
                {
                    error = "STR_OZ_ERR_MARKERS_FULL";
                    return "";
                }
                marksTaken = 0;
                marksTotal = incoming.Items.Count();
            }
            else
            {

            int total = incoming.Items.Count();
            int taken = 0;
            for (int i = 0; i < total; i++)
            {
                if (mine.Items.Count() >= limit)
                    break;

                OZ_MapMarker m = incoming.Items[i];
                // Чужий чип -- чужий JSON: масив може нести null-елементи.
                if (!m)
                    continue;

                // Той самий санітар, що й у marker_add: чуже походження --
                // не привілей, а межі в чипа ніхто не питав.
                m.Name = OZ_Text.Clip(m.Name, OZ_PdaTune.MarkerNameMax());
                m.Desc = OZ_Text.Clip(m.Desc, OZ_PdaTune.MarkerDescMax());

                // Дедуп за ВМІСТОМ: та сама назва в тій самій точці вже на
                // пристрої -- не дублюємо. Без цього резервна копія (записав
                // усі мітки на чип, потім імпортував) плодила б другий
                // комплект, а цикл експорт->імпорт множив мітку щоразу.
                bool dup = false;
                for (int d = 0; d < mine.Items.Count(); d++)
                {
                    if (mine.Items[d].Name == m.Name && mine.Items[d].Pos == m.Pos)
                    {
                        dup = true;
                        break;
                    }
                }
                if (dup)
                    continue;

                // Id карбуємо ЗАНОВО: чужі id зіткнулись би з нашими, і
                // видалення по id зносило б не ту мітку.
                s_CarrierSeq++;
                m.Id = OZ_Time.NowUtc() + "#c" + s_CarrierSeq.ToString();
                mine.Items.Insert(m);
                taken++;
            }

            string outJson;
            if (!JsonFileLoader<OZ_MarkerList>.MakeData(mine, outJson, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            pda.OZ_SetMarkersJson(outJson);
            OZ_Log.Info("carrier: imported " + taken.ToString() + "/" + total.ToString() + " marker(s) for " + sender.GetPlainId());

            marksTaken = taken;
            marksTotal = total;
            }
        }

        if (c.OZ_Notes() != "")
        {
            OZ_NoteBook book;
            string err2;
            if (!JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Notes(), book, err2) || !book || !book.Notes)
            {
                // Мітки вже ЛЯГЛИ. Бита книжка чипа не має права стерти цей
                // факт: чесно віддаємо підсумок міток, записки лишаються на
                // чипі на потім.
                if (marksTotal >= 0)
                    return MarksOnlyTaken(marksTaken, marksTotal, ok, error);
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            // Записки -- пам'ять ПРИСТРОЮ: книжка чипа зливається в книжку
            // приладу тут же, синхронно. Межі й дедап ті самі, що в міток,
            // а підсумок один на обидві ноги -- одна op, одна цифра.
            OZ_PDA_Base pdaN = OZ_PdaLookup.HeldBy(sender);
            OZ_PdaProfile profN = OZ_PdaProfiles.ForClass(pdaN.GetType());

            OZ_NoteBook mineN = new OZ_NoteBook();
            if (pdaN.OZ_NotesJson() != "")
            {
                OZ_NoteBook parsedN;
                if (JsonFileLoader<OZ_NoteBook>.LoadData(pdaN.OZ_NotesJson(), parsedN, err2) && parsedN && parsedN.Notes)
                    mineN = parsedN;
            }

            int limitN = 0;
            if (profN && profN.Limits)
                limitN = profN.Limits.Notes;
            if (limitN <= 0)
                limitN = OZ_PdaTune.NotesMax();

            int totalN = book.Notes.Count();
            int takenN = 0;
            for (int ni = 0; ni < totalN; ni++)
            {
                if (mineN.Notes.Count() >= limitN)
                    break;

                OZ_Note nn = book.Notes[ni];
                // Чужий чип -- чужий JSON: масив може нести null-елементи.
                if (!nn)
                    continue;

                string tN = OZ_Text.Clip(nn.Title, OZ_PdaTune.NoteTitleMax());
                string bN = OZ_Text.Clip(nn.Body, OZ_PdaTune.NoteBodyMax());

                // Дедуп за ВМІСТОМ: цикл експорт->імпорт не плодить копій.
                bool dupN = false;
                for (int nd = 0; nd < mineN.Notes.Count(); nd++)
                {
                    if (mineN.Notes[nd].Title == tN && mineN.Notes[nd].Body == bN)
                    {
                        dupN = true;
                        break;
                    }
                }
                if (dupN)
                    continue;

                OZ_Note freshI = new OZ_Note();
                s_CarrierSeq++;
                freshI.Id        = OZ_Time.NowUtc() + "#n" + s_CarrierSeq.ToString();
                freshI.Title     = tN;
                freshI.Body      = bN;
                freshI.CreatedAt = OZ_Time.NowUtc();
                freshI.EditedAt  = freshI.CreatedAt;
                mineN.Notes.Insert(freshI);
                takenN++;
            }

            string outN;
            if (!JsonFileLoader<OZ_NoteBook>.MakeData(mineN, outN, err2, false))
            {
                if (marksTotal >= 0)
                    return MarksOnlyTaken(marksTaken, marksTotal, ok, error);
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            pdaN.OZ_SetNotesJson(outN);
            OZ_Log.Info("carrier: imported " + takenN.ToString() + "/" + totalN.ToString() + " note(s) for " + sender.GetPlainId());

            OZ_CarrierTaken tAll = new OZ_CarrierTaken();
            tAll.Taken = takenN;
            tAll.Total = totalN;
            if (marksTotal > 0)
            {
                tAll.Taken += marksTaken;
                tAll.Total += marksTotal;
            }

            string tjAll;
            if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(tAll, tjAll, err2, false))
                tjAll = "";

            ok = true;
            error = "";
            return tjAll;
        }

        // Лише мітки: відповідь синхронна.
        if (marksTotal >= 0)
        {
            OZ_CarrierTaken t = new OZ_CarrierTaken();
            t.Taken = marksTaken;
            t.Total = marksTotal;

            string tjm;
            string terr;
            if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(t, tjm, terr, false))
                tjm = "";

            ok = true;
            error = "";
            return tjm;
        }

        error = "STR_OZ_ERR_INTERNAL";
        return "";
    }

    // Підсумок «лише мітки» для гуртового імпорту, коли нотаткова нога
    // відмовила синхронно: зроблене вже зроблене, і відповідь каже саме це.
    private string MarksOnlyTaken(int taken, int total, out bool ok, out string error)
    {
        OZ_CarrierTaken t = new OZ_CarrierTaken();
        t.Taken = taken;
        t.Total = total;

        string tj;
        string terr;
        if (!JsonFileLoader<OZ_CarrierTaken>.MakeData(t, tj, terr, false))
            tj = "";

        ok = true;
        error = "";
        return tj;
    }

    // Забрати ОДИН запис із чипа: гравець дивиться превʼю і бере лише те,
    // що йому треба. І мітка, і записка лягають одразу в пам'ять пристрою.
    private string CarrierTake(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_CarrierItemRef r;
        string err;
        if (!JsonFileLoader<OZ_CarrierItemRef>.LoadData(json, r, err) || !r || r.Index < 0)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        if (r.Kind == "mark")
        {
            OZ_MarkerList src;
            if (!JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Marks(), src, err) || !src || !src.Items || r.Index >= src.Items.Count())
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
            OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());

            OZ_MarkerList mine = new OZ_MarkerList();
            string raw = pda.OZ_MarkersJson();
            if (raw != "")
            {
                OZ_MarkerList parsed;
                if (JsonFileLoader<OZ_MarkerList>.LoadData(raw, parsed, err) && parsed && parsed.Items)
                    mine = parsed;
            }

            int limit = 0;
            if (prof && prof.Limits)
                limit = prof.Limits.Markers;

            if (limit <= 0 || mine.Items.Count() >= limit)
            {
                error = "STR_OZ_ERR_MARKERS_FULL";
                return "";
            }

            OZ_MapMarker m = src.Items[r.Index];
            if (!m)
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }
            m.Name = OZ_Text.Clip(m.Name, OZ_PdaTune.MarkerNameMax());
            m.Desc = OZ_Text.Clip(m.Desc, OZ_PdaTune.MarkerDescMax());

            // Дедуп за ВМІСТОМ -- та сама причина, що в гуртового імпорту:
            // цикл експорт->імпорт не має плодити копії.
            for (int d = 0; d < mine.Items.Count(); d++)
            {
                if (mine.Items[d].Name == m.Name && mine.Items[d].Pos == m.Pos)
                {
                    error = "STR_OZ_ERR_CARRIER_DUP";
                    return "";
                }
            }

            s_CarrierSeq++;
            m.Id = OZ_Time.NowUtc() + "#c" + s_CarrierSeq.ToString();
            mine.Items.Insert(m);

            string outJson;
            if (!JsonFileLoader<OZ_MarkerList>.MakeData(mine, outJson, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            pda.OZ_SetMarkersJson(outJson);
            ok = true;
            error = "";
            return "";
        }

        if (r.Kind == "note")
        {
            OZ_NoteBook book;
            if (!JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Notes(), book, err) || !book || !book.Notes || r.Index >= book.Notes.Count())
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            if (!book.Notes[r.Index])
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            // Записки -- пам'ять ПРИСТРОЮ: забрати з чипа означає
            // дописати в книжку приладу, без моста. Межі й дедап ті
            // самі, що в міток: цикл експорт->імпорт не плодить копій.
            OZ_PDA_Base pdaN = OZ_PdaLookup.HeldBy(sender);
            OZ_PdaProfile profN = OZ_PdaProfiles.ForClass(pdaN.GetType());

            OZ_NoteBook mineN = new OZ_NoteBook();
            if (pdaN.OZ_NotesJson() != "")
            {
                OZ_NoteBook parsedN;
                if (JsonFileLoader<OZ_NoteBook>.LoadData(pdaN.OZ_NotesJson(), parsedN, err) && parsedN && parsedN.Notes)
                    mineN = parsedN;
            }

            int limitN = 0;
            if (profN && profN.Limits)
                limitN = profN.Limits.Notes;
            if (limitN <= 0)
                limitN = OZ_PdaTune.NotesMax();

            if (mineN.Notes.Count() >= limitN)
            {
                error = "STR_OZ_ERR_NOTES_FULL";
                return "";
            }

            string tN = OZ_Text.Clip(book.Notes[r.Index].Title, OZ_PdaTune.NoteTitleMax());
            string bN = OZ_Text.Clip(book.Notes[r.Index].Body, OZ_PdaTune.NoteBodyMax());

            for (int dn = 0; dn < mineN.Notes.Count(); dn++)
            {
                if (mineN.Notes[dn].Title == tN && mineN.Notes[dn].Body == bN)
                {
                    error = "STR_OZ_ERR_CARRIER_DUP";
                    return "";
                }
            }

            OZ_Note freshN = new OZ_Note();
            s_CarrierSeq++;
            freshN.Id        = OZ_Time.NowUtc() + "#n" + s_CarrierSeq.ToString();
            freshN.Title     = tN;
            freshN.Body      = bN;
            freshN.CreatedAt = OZ_Time.NowUtc();
            freshN.EditedAt  = freshN.CreatedAt;
            mineN.Notes.Insert(freshN);

            string outN;
            if (!JsonFileLoader<OZ_NoteBook>.MakeData(mineN, outN, err, false))
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            pdaN.OZ_SetNotesJson(outN);
            ok = true;
            error = "";
            return "";
        }

        error = "STR_OZ_ERR_INTERNAL";
        return "";
    }

    // Стерти ОДИН запис із чипа. Писабельність та сама, що в будь-якого
    // запису: замкнений клас не редагується поштучно так само, як і цілком.
    private string CarrierDel(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_CarrierItemRef r;
        string err;
        if (!JsonFileLoader<OZ_CarrierItemRef>.LoadData(json, r, err) || !r || r.Index < 0)
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        OZ_DataCarrier_Base c = OZ_CarrierOps.ResolveWritable(sender, error);
        if (!c)
            return "";

        if (r.Kind == "mark")
        {
            OZ_MarkerList ml;
            if (!JsonFileLoader<OZ_MarkerList>.LoadData(c.OZ_Marks(), ml, err) || !ml || !ml.Items || r.Index >= ml.Items.Count())
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            ml.Items.RemoveOrdered(r.Index);

            string mj = "";
            if (ml.Items.Count() > 0)
            {
                if (!JsonFileLoader<OZ_MarkerList>.MakeData(ml, mj, err, false))
                {
                    error = "STR_OZ_ERR_INTERNAL";
                    return "";
                }
            }

            c.OZ_WriteMarks(mj, ml.Items.Count());
            ok = true;
            error = "";
            return "";
        }

        if (r.Kind == "note")
        {
            OZ_NoteBook nb;
            if (!JsonFileLoader<OZ_NoteBook>.LoadData(c.OZ_Notes(), nb, err) || !nb || !nb.Notes || r.Index >= nb.Notes.Count())
            {
                error = "STR_OZ_ERR_INTERNAL";
                return "";
            }

            nb.Notes.RemoveOrdered(r.Index);

            string nj = "";
            if (nb.Notes.Count() > 0)
            {
                if (!JsonFileLoader<OZ_NoteBook>.MakeData(nb, nj, err, false))
                {
                    error = "STR_OZ_ERR_INTERNAL";
                    return "";
                }
            }

            c.OZ_WriteNotes(nj, nb.Notes.Count());
            ok = true;
            error = "";
            return "";
        }

        error = "STR_OZ_ERR_INTERNAL";
        return "";
    }

    private string CarrierErase(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_DataCarrier_Base c = CarrierOf(sender, error);
        if (!c)
            return "";

        OZ_CarrierSpec spec = OZ_PdaHardware.CarrierFor(c.GetType());
        if (!spec || !spec.Writable)
        {
            error = "STR_OZ_ERR_CARRIER_LOCKED";
            return "";
        }

        // Симетрія з імпортом і читанням: порожній чип стирати нема чого,
        // і «зроблено» на ніщо було б звітом про неіснуючу роботу.
        if (!c.OZ_IsWritten())
        {
            error = "STR_OZ_ERR_CARRIER_BLANK";
            return "";
        }

        c.OZ_Erase();
        ok = true;
        error = "";
        return "";
    }

    private static int s_CarrierSeq = 0;

    private string Status(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (!prof)
        {
            // Пристрій є, а профілю під нього немає: адмін прибрав його з
            // Profiles.json або переплутав класнейм. Кажемо про це прямо.
            OZ_Log.Warn("no device profile for class " + pda.GetType());
            error = "STR_OZ_ERR_NO_PROFILE";
            return "";
        }

        PlayerBase player = OZ_PdaLookup.PlayerOf(sender);
        OZ_PlayerData pd = OZ_PlayerStore.Load(sender.GetPlainId());

        // Замок тут НЕ рахуємо: OZ_PdaAccess.Check уже покликав
        // OZ_EvaluateLock мікросекундами раніше, на цьому ж самому запиті
        // (OZ_PdaAccess.c:42) -- і робить це для КОЖНОЇ сторінки, а не лише
        // для цієї. Другий виклик тут був мертвою роботою, і він же створював
        // хибне враження, ніби автозамок тримається на секундному опитуванні
        // сторінки «Пристрій». Не тримається: він у воротах.

        // Ліниво, як і замок: рахунок дешифратора добігає саме тоді, коли на
        // пристрій дивляться.
        pda.OZ_EvaluateCrack();

        // Прив'язки «поглядом» більше немає: власність дає лише явна
        // ІНІЦІАЦІЯ (op initiate). Пристрій без сесії -- чесно нічий.

        OZ_PdaDeviceStatus st = new OZ_PdaDeviceStatus();

        st.ClassName   = pda.GetType();
        st.ProfileId   = prof.Id;
        st.DisplayName = prof.DisplayName;
        st.ModuleSlots = prof.ModuleSlots;
        st.LockAfterMinutes = prof.LockAfterMinutes;

        // Адреса пристрою для клієнта. GetNetworkID віддає id двома int'ами,
        // і клієнт піднімає по них ту саму сутність через GetObjectByNetworkId.
        int netLow;
        int netHigh;
        pda.GetNetworkID(netLow, netHigh);
        st.NetLow  = netLow;
        st.NetHigh = netHigh;
        st.InHands = (player != null && player.GetItemInHands() == pda);

        // Чужому чи неініційованому пристрою -- лише ЙОГО власні вкладки:
        // пристрій і карта. Акаунтні сторінки все одно відіб'є гейт, а
        // мертві вкладки в стрічці лише брехали б.
        bool ownedAtAll = pda.OZ_HasAnySession();
        bool devFrozen  = OZ_PdaCapsule.IsFrozen(pda);

        for (int i = 0; i < prof.Pages.Count(); i++)
        {
            // На клієнт їдуть лише ті сторінки, які СПРАВДІ зареєстровані:
            // намалювати вкладку, за якою нікого немає, гірше, ніж не
            // намалювати її зовсім.
            if (!OZ_PageRegistry.Has(prof.Pages[i]))
                continue;

            if (prof.Pages[i] != OZ_PdaConst.PAGE_DEVICE)
            {
                // Нічийному -- лише пристрій. КАПСУЛІ -- читальня: карта,
                // розмови, записки (зрізом до заморозки). ЖИВИЙ говорить
                // за власника сесії повним набором, хто б його не тримав.
                if (!ownedAtAll)
                    continue;
                if (devFrozen && prof.Pages[i] != OZ_PdaConst.PAGE_MAP && prof.Pages[i] != OZ_PdaConst.PAGE_CHAT && prof.Pages[i] != OZ_PdaConst.PAGE_NOTES && prof.Pages[i] != OZ_PdaConst.PAGE_CONTACTS)
                    continue;
            }

            st.Pages.Insert(prof.Pages[i]);
        }

        // І сторінки, які приносить ЗАЛІЗО.
        //
        // Профіль описує ПРИСТРІЙ, а не те, що в нього вставили, тож без
        // цього договір EnablesPages лишався б обіцянкою, якої КПК не
        // виконує. Саме так і сталося з рацією: гейт операцій уже питав
        // модулі (OZ_PdaAccess.ModuleEnables), а перелік вкладок -- ні, і
        // вставлена плата працювала б, якби до неї було як дійти.
        for (int m = 0; m < OZ_PdaConst.MODULE_SLOTS_MAX; m++)
        {
            string mcls = pda.OZ_ModuleClass(m);
            if (mcls == "")
                continue;

            OZ_ModuleSpec mspec = OZ_PdaHardware.ModuleFor(mcls);
            if (!mspec || !mspec.EnablesPages)
                continue;

            for (int e = 0; e < mspec.EnablesPages.Count(); e++)
            {
                string extra = mspec.EnablesPages[e];
                if (!OZ_PageRegistry.Has(extra))
                    continue;
                if (st.Pages.Find(extra) != -1)
                    continue;
                st.Pages.Insert(extra);
            }
        }

        st.Powered    = pda.OZ_IsOn();
        st.HasBattery = pda.OZ_HasBattery();
        st.Charge01   = pda.OZ_Charge01();

        for (int b = 0; b < OZ_PdaConst.MODULE_SLOTS_MAX; b++)
        {
            OZ_BayInfo bay = new OZ_BayInfo();
            bay.Index   = b;
            bay.Visible = (b < prof.ModuleSlots);

            string cls = pda.OZ_ModuleClass(b);
            if (cls != "")
            {
                bay.ClassName = cls;
                OZ_ModuleSpec spec = OZ_PdaHardware.ModuleFor(cls);
                if (spec)
                {
                    bay.Display = spec.DisplayName;
                    bay.Kind    = spec.Kind;
                }
            }
            st.Bays.Insert(bay);
        }

        st.CarrierClass = pda.OZ_CarrierClass();
        if (st.CarrierClass != "")
        {
            OZ_CarrierSpec cs = OZ_PdaHardware.CarrierFor(st.CarrierClass);
            if (cs)
            {
                st.CarrierWritable = cs.Writable;
                st.CarrierDisplay  = cs.DisplayName;
                st.CarrierMaxMarks = cs.MaxMarks;
                st.CarrierMaxNotes = cs.MaxNotes;
            }

            // ВМІСТ чипа -- лише на УВІМКНЕНОМУ пристрої. Наявність носія
            // видно фізично (клас вище), а от що на ньому записано і скільки
            // -- це вже читання, і мертвий КПК його не робить. Інакше
            // знайдений вимкнений прилад видавав би вміст чужого чипа тим
            // самим статусом, у якому carrier_read чесно відмовляє.
            OZ_DataCarrier_Base carrier = OZ_DataCarrier_Base.Cast(pda.OZ_Attached(OZ_PdaConst.SLOT_CARRIER));
            if (carrier && st.Powered)
            {
                st.CarrierWritten = carrier.OZ_IsWritten();
                st.CarrierMarks   = carrier.OZ_MarkCount();
                st.CarrierNotes   = carrier.OZ_NoteCount();
            }
        }

        st.HasPin    = pda.OZ_HasPin();
        st.Unlocked  = pda.OZ_IsUnlocked();
        st.AutoLock  = pda.OZ_AutoLock();
        st.ForceAutoLock = prof.ForceAutoLock;
        st.LockedOut = pda.OZ_IsLockedOut(sender.GetPlainId());
        st.LockWaitS = pda.OZ_LockWaitSec(sender.GetPlainId());

        st.Sealed       = pda.OZ_IsSealed();
        st.HasDecryptor = pda.OZ_HasDecryptor();
        st.Cracking     = pda.OZ_IsCracking();
        st.CrackLeftSec = pda.OZ_CrackLeftSec();

        // Сесія й прив'язка -- теж читання, і теж лише на увімкненому.
        // Хто востаннє тримав пристрій живим і чи прив'язаний його акаунт --
        // не те, що видно ззовні з мертвого приладу.
        if (st.Powered)
        {
            // Онлайн міряється епохою ВЛАСНИКА сесії, не глядача: вкрадений
            // КПК живого власника має чесно казати «онлайн у нього», а
            // капсула -- лишатись капсулою в будь-чиїх руках.
            string ownUid = pda.OZ_SessionUid();
            int ownEpoch = 0;
            OZ_PlayerData ownPd = null;
            if (ownUid != "")
            {
                ownPd = OZ_PlayerStore.Load(ownUid);
                ownEpoch = ownPd.SessionEpoch;
                st.OwnerName = ownPd.Name;
            }

            st.Owned       = pda.OZ_HasAnySession();
            st.Online      = pda.OZ_IsOnline(ownEpoch);
            st.SessionMine = pda.OZ_HasSession(sender.GetPlainId(), pd.SessionEpoch);

            if (st.Online && ownPd)
            {
                // Живий пристрій наповнює свою майбутню капсулу даними
                // ВЛАСНИКА СЕСІЇ -- пристрій говорить за нього, хто б не
                // тримав. Штамп цього запису -- заразом МИТЬ ЗАМОРОЗКИ:
                // коли епоха власника піде вперед, зріз історії ріжеться
                // саме по ньому. Пишеться на кожен статус -- це пам'ять до
                // першого сейву, дешевше за будь-який дросель.
                OZ_PdaSnapshot snap = new OZ_PdaSnapshot();
                snap.Owner   = ownPd.Name;
                snap.Faction = OZ_Factions.NameOf(OZ_Factions.OfUid(ownUid));
                for (int sf = 0; sf < ownPd.Friends.Count(); sf++)
                {
                    // Ім'я з ТОГО САМОГО персонажа, а не з акаунта: у капсулі
                    // мусить лишитись той, кого власник знав.
                    OZ_PlayerData fr = OZ_PlayerStore.ByKey(ownPd.Friends[sf]);
                    if (fr && fr.Name != "")
                        snap.Contacts.Insert(fr.Name);
                }

                string snapJson;
                string snapErr;
                if (JsonFileLoader<OZ_PdaSnapshot>.MakeData(snap, snapJson, snapErr, false))
                    pda.OZ_RefreshSnapshot(ownEpoch, snapJson);
            }

            if (!st.Online)
            {
                st.SnapshotAt = pda.OZ_SnapshotAt();
                st.Snapshot   = pda.OZ_Snapshot();
            }

            st.DiscordLinked = (pd.DiscordId != "");
            st.FirstSeen     = pd.FirstSeen;
        }

        // Радіацію питаємо ЛИШЕ якщо є чим міряти. Питати те, чого нема чим
        // виміряти, і малювати відповідь -- це вигадувати цифри.
        // ...і лише якщо пристрій УВІМКНЕНО. Замірник живиться від нього, і
        // вимкнений КПК, який показує поточний фон, -- це не прилад.
        bool wantAmbient = st.Powered && pda.OZ_HasModuleKind(OZ_PdaConst.MOD_RADIOMETER);
        bool wantDose    = st.Powered && pda.OZ_HasModuleKind(OZ_PdaConst.MOD_DOSIMETER);
        if (wantAmbient || wantDose)
        {
            OZ_RadiationReading rr = OZ_PdaRadiation.Read(player, wantAmbient, wantDose);
            st.HasRadiationProvider = rr.HasProvider;
            st.AmbientUSvH = rr.AmbientUSvH;
            st.DoseUSv     = rr.DoseUSv;
            st.DoseWarnUSv = rr.DoseWarnUSv;
        }

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.MakeData(st, outJson, err, false))
        {
            OZ_Log.Error("device status serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    private string Unlock(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaPinAttempt att;
        string err;
        if (!JsonFileLoader<OZ_PdaPinAttempt>.LoadData(json, att, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (!pda.OZ_TryUnlock(sender.GetPlainId(), att.Pin))
        {
            // Скільки спроб лишилось -- НЕ кажемо. Це підказка тому, хто
            // підбирає, і жодної користі власнику.
            error = "STR_OZ_ERR_BAD_PIN";
            return "";
        }

        // Знати пін -- НЕ означає володіти: прив'язка лишається за
        // ініціатором (рішення власника 2026-08-28). Сесію відкриває лише
        // бесхазяйний пристрій -- та сама умова, що й у setpin.
        OZ_PlayerData pd = OZ_PlayerStore.Load(sender.GetPlainId());
        if (!pda.OZ_HasAnySession())
            pda.OZ_OpenSession(sender.GetPlainId(), pd.SessionEpoch);

        ok = true;
        error = "";
        return "";
    }

    // Що можна сказати про ЗАМКНЕНИЙ пристрій, не відмикаючи його.
    //
    // Рівно чотири речі, і жодна з них нічого не видає: чи він запечатаний,
    // чи є чим його зламати, чи вже ламають і скільки лишилось. Усе решта --
    // ім'я, профіль, вміст -- лишається за замком, бо саме за цим замок і є.
    private string Sealed(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        pda.OZ_EvaluateCrack();

        OZ_PdaDeviceStatus st = new OZ_PdaDeviceStatus();
        st.Sealed       = pda.OZ_IsSealed();
        st.HasDecryptor = pda.OZ_HasDecryptor();
        st.Cracking     = pda.OZ_IsCracking();
        st.CrackLeftSec = pda.OZ_CrackLeftSec();

        string outJson;
        string err;
        if (!JsonFileLoader<OZ_PdaDeviceStatus>.MakeData(st, outJson, err, false))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        ok = true;
        error = "";
        return outJson;
    }

    // Почати злам запечатаного пристрою.
    private string Crack(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (!prof)
        {
            error = "STR_OZ_ERR_NO_PROFILE";
            return "";
        }

        string why = pda.OZ_StartCrack(prof.CrackSeconds);
        if (why != "")
        {
            error = why;
            return "";
        }

        ok = true;
        error = "";
        return "";
    }

    // Зміна коду. Щоб змінити пін, його треба ЗНАТИ -- пристрій не питає,
    // хто ти, він питає старий код. Порожній новий код означає «зняти пін».
    private string SetPin(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaPinChange ch;
        string err;
        if (!JsonFileLoader<OZ_PdaPinChange>.LoadData(json, ch, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (!pda.OZ_SetPin(sender.GetPlainId(), ch.OldPin, ch.NewPin))
        {
            // Скільки спроб лишилось -- НЕ кажемо, з тієї ж причини, що й при
            // відмиканні: це підказка тому, хто підбирає.
            error = "STR_OZ_ERR_BAD_PIN";
            return "";
        }

        // Пін -- це ЗАМОК, не власність: сесію дає лише явна ініціація.

        ok = true;
        error = "";
        return "";
    }

    // ІНІЦІАЦІЯ: явна церемонія власності. Пристрій прив'язується до
    // ПОТОЧНОЇ епохи гравця і стає ще одним його живим терміналом --
    // попередні НЕ гаснуть (рішення власника 2026-08-28). Гасить їх лише
    // явний LOG OUT OTHER DEVICES.
    private string Initiate(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        if (pda.OZ_HasAnySession())
        {
            error = "STR_OZ_ERR_OWNED";
            return "";
        }

        string uid = sender.GetPlainId();
        OZ_PlayerData pd = OZ_PlayerStore.Load(uid);

        pda.OZ_OpenSession(uid, pd.SessionEpoch);

        OZ_Log.Info("pda: " + uid + " initiated " + pda.GetType() + ", epoch " + pd.SessionEpoch.ToString());

        ok = true;
        error = "";
        return "";
    }

    // «Розлогінитись на інших»: епоха +1 і перевідкриття СВОЄЇ сесії тут
    // же -- всі інші пристрої власника замерзають, цей лишається єдиним
    // живим. Вимагає живої сесії саме на цьому пристрої.
    private string LogoutOthers(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        string uid = sender.GetPlainId();
        OZ_PlayerData pd = OZ_PlayerStore.Load(uid);

        if (!pda.OZ_HasSession(uid, pd.SessionEpoch))
        {
            error = "STR_OZ_ERR_NOT_YOURS";
            return "";
        }

        pd.SessionEpoch = pd.SessionEpoch + 1;
        OZ_PlayerStore.Flush(uid);
        pda.OZ_OpenSession(uid, pd.SessionEpoch);

        OZ_Log.Info("pda: " + uid + " logged out other devices, epoch " + pd.SessionEpoch.ToString());

        ok = true;
        error = "";
        return "";
    }

    // Ручний замок: власник іде від пристрою -- пристрій мовчить одразу,
    // а не за таймером автолока.
    private string Lock(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        if (!pda.OZ_HasPin())
        {
            error = "STR_OZ_ERR_NO_PIN";
            return "";
        }

        pda.OZ_Lock();
        ok = true;
        error = "";
        return "";
    }

    // «До заводських» без пінa -- переініціалізація знайденого пристрою.
    // Дані попереднього власника згорають чесно й повністю; Sealed
    // відмовляє всередині предмета.
    private string FactoryReset(PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        string why = pda.OZ_FactoryReset();
        if (why != "")
        {
            error = why;
            return "";
        }

        OZ_Log.Info("pda: factory reset by " + sender.GetPlainId());
        ok = true;
        error = "";
        return "";
    }

    // Живлення. Той самий важіль, що й ванільна дія з рук -- але через сервер:
    // клієнт просить, сервер вирішує й називає причину відмови.
    private string Power(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaFlagOp flag;
        string err;
        if (!JsonFileLoader<OZ_PdaFlagOp>.LoadData(json, flag, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        string why = pda.OZ_SetPower(flag.Value);
        if (why != "")
        {
            error = why;
            return "";
        }

        ok = true;
        error = "";
        return "";
    }

    private string AutoLock(string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;

        OZ_PDA_Base pda = OZ_PdaLookup.HeldBy(sender);
        if (!pda)
        {
            error = "STR_OZ_ERR_NO_DEVICE";
            return "";
        }

        OZ_PdaProfile prof = OZ_PdaProfiles.ForClass(pda.GetType());
        if (!prof)
        {
            error = "STR_OZ_ERR_NO_PROFILE";
            return "";
        }

        OZ_PdaFlagOp flag;
        string err;
        if (!JsonFileLoader<OZ_PdaFlagOp>.LoadData(json, flag, err))
        {
            error = "STR_OZ_ERR_INTERNAL";
            return "";
        }

        if (!pda.OZ_SetAutoLock(flag.Value, prof.ForceAutoLock))
        {
            error = "STR_OZ_ERR_REFUSED";
            return "";
        }

        ok = true;
        error = "";
        return "";
    }
}

class OZ_PdaHandlerQuests : OZ_PageHandler
{
    override string Handle(string op, string json, PlayerIdentity sender, out bool ok, out string error)
    {
        ok = false;
        error = "STR_OZ_ERR_UNKNOWN_OP";

        if (op == "journal")
        {
            // Порожній журнал і ВІДСУТНІЙ журнал -- різні повідомлення для
            // гравця. HasProvider розрізняє «завдань немає» і «на цьому
            // сервері квестового мода взагалі немає».
            OZ_QuestJournal j = OZ_PdaQuests.Collect(sender);

            string outJson;
            string err;
            if (JsonFileLoader<OZ_QuestJournal>.MakeData(j, outJson, err, false))
            {
                ok = true;
                error = "";
                return outJson;
            }

            OZ_Log.Error("quest journal serialise failed: " + err);
            error = "STR_OZ_ERR_INTERNAL";
        }

        return "";
    }
}

[CF_RegisterModule(OZ_PdaModule)]
class OZ_PdaModule : CF_ModuleWorld
{
    private ref Timer m_BeaconTimer;

    void BeaconTick()
    {
        OZ_PdaHandlerMap.PushBeacons();
    }

    override void OnInit()
    {
        super.OnInit();
        EnableMissionStart();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        if (!GetGame().IsServer())
            return;

        // Дерево каталогів профілю -- ПЕРШИМ рядком, до будь-якого читання
        // чи запису. Ядро будує його у своєму OnMissionStart, але порядок
        // CF-модулів не гарантований, і на цьому ж стенді він уже підводив:
        // рація відпрацювала раніше за КПК. EnsureTree ідемпотентна, тож
        // зайвий виклик коштує нічого, а відсутній коштує конфігів.
        OZ_Json.EnsureTree();

        // Спочатку СТОРІНКИ, потім профілі: Validate() профілів звіряє свій
        // список Pages з реєстром, і на порожньому реєстрі виплюнув би
        // попередження на кожен рядок.
        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_DEVICE,
                                 "#STR_OZ_PAGE_DEVICE",
                                 "set:oz_pda image:device",
                                 new OZ_PdaHandlerDevice());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_QUESTS,
                                 "#STR_OZ_PAGE_QUESTS",
                                 "set:oz_pda image:quests",
                                 new OZ_PdaHandlerQuests());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_CONTACTS,
                                 "#STR_OZ_PAGE_CONTACTS",
                                 "set:oz_pda image:contacts",
                                 new OZ_PdaHandlerContacts());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_NOTES,
                                 "#STR_OZ_PAGE_NOTES",
                                 "set:oz_pda image:notes",
                                 new OZ_PdaHandlerNotes());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_MAP,
                                 "#STR_OZ_PAGE_MAP",
                                 "set:oz_pda image:map",
                                 new OZ_PdaHandlerMap());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_CHAT,
                                 "#STR_OZ_PAGE_CHAT",
                                 "set:oz_pda image:chat",
                                 new OZ_PdaHandlerChat());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_FACTION,
                                 "#STR_OZ_PAGE_FACTION",
                                 "set:oz_pda image:faction",
                                 new OZ_PdaHandlerFaction());

        OZ_PageRegistry.Register(OZ_PdaConst.PAGE_NEWS,
                                 "#STR_OZ_PAGE_NEWS",
                                 "set:oz_pda image:news",
                                 new OZ_PdaHandlerNews());

        // Розмови живуть у Discord, тож на диску їм каталогу не треба -- а
        // ось вухо для вхідних рядків треба. Підписка не залежить від того,
        // чи вже стартував міст: порядок модулів CF не гарантований, а мапа
        // приймачів однаково питається на кожну пачку.
        OZ_BridgeClient.Subscribe("chat", new OZ_ChatSink());
        OZ_BridgeClient.Subscribe("news", new OZ_NewsSink());
        OZ_RoleNotify.On().Insert(OZ_PdaRolePush.Changed);
        OZ_PdaModules.Register(new OZ_SpyAntennaBehaviour());
        OZ_BridgeClient.RegisterUidProvider(new OZ_PdaUidProvider());

        OZ_PdaProfiles.ServerLoad();
        OZ_PdaHardware.ServerLoad();
        OZ_PdaTuning.ServerLoad();

        // Конфіги КПК стають редагованими з адмінської консолі ядра.
        OZ_AdminCfg.Register("Tuning",   OZ_Const.PROFILE_DIR + "\\Tuning.json",   new OZ_PdaTuningApplier(), "pda");
        OZ_AdminCfg.Register("Profiles", OZ_PdaConst.PROFILES, new OZ_PdaProfilesApplier(), "pda");
        OZ_AdminCfg.Register("Hardware", OZ_PdaConst.HARDWARE, new OZ_PdaHardwareApplier(), "pda");

        // Маячки транспондера РОЗСИЛАЄ сервер -- раз на кілька секунд тим,
        // у кого антена справді працює. Клієнт більше нічого не опитує:
        // на сорока гравцях це мінус десять запитів на секунду.
        m_BeaconTimer = new Timer(CALL_CATEGORY_SYSTEM);
        m_BeaconTimer.Run(OZ_PdaTune.BeaconPushSeconds(), this, "BeaconTick", NULL, true);
        OZ_Factions.ServerLoad();

        // Ядро пускало всі сторінки, бо пристроїв не має. Тепер вирішує той,
        // хто їх приносить.
        OZ_PageAccess.Bind(new OZ_PdaAccess());

        CheckSlots();

        string summary = "pda loaded: profiles=" + OZ_PdaProfiles.Count().ToString();
        summary += " pages=" + OZ_PageRegistry.Count().ToString();
        summary += " modules=" + OZ_PdaHardware.ModuleCount().ToString();
        summary += " factions=" + OZ_Factions.Count().ToString();
        summary += " carriers=" + OZ_PdaHardware.CarrierCount().ToString();
        OZ_Log.Info(summary);
    }

    // Слот з друкарською помилкою в імені -- класична мовчазна поломка: конфіг
    // парситься, предмет спавниться, а вкласти в нього нічого не можна, і в
    // лозі про це ані слова. Ловимо на буті, а не в грі.
    private void CheckSlots()
    {
        CheckSlot(OZ_PdaConst.SLOT_BATTERY);
        CheckSlot(OZ_PdaConst.SLOT_CARRIER);
        CheckSlot(OZ_PdaConst.SLOT_WEAR);
        for (int i = 0; i < OZ_PdaConst.MODULE_SLOTS_MAX; i++)
            CheckSlot(OZ_PdaConst.ModuleSlot(i));
    }

    private void CheckSlot(string name)
    {
        int id = InventorySlots.GetSlotIdFromString(name);
        if (id == -1)
        {
            OZ_Log.Warn("slot \"" + name + "\" does not resolve - check CfgSlots and attachments[]");
            return;
        }
        OZ_Log.Dbg("slot " + name + " -> id " + id.ToString());
    }
}
