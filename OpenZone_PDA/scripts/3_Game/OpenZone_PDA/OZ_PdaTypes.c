// Форми відповідей сторінок КПК. Живуть у 3_Game, бо їх серіалізує сервер
// (4_World) і читає клієнт (5_Mission) -- спільним для обох є лише цей шар.

class OZ_PdaDeviceStatus
{
    bool   DiscordLinked = false;
    string FirstSeen     = "";
}
