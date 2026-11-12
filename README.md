# OpenFO — GitHub Actions Windows x64 CI

Репозиторій для двох незалежних Windows x64 збірок OpenFO: **baseline_084** і **optimized_093**.
Кожна компіляція створює **OpenFO.exe (OpenGL)**, **OpenFO_D3D9.exe (D3D9Ex)**,
обидві діагностичні версії та **OpenFOValidate.exe**. Після успішної компіляції
запускаються швидкі автономні C++ тести.

## Як перенести до GitHub (один раз)

1. Розпакуй ZIP на Windows у нову папку, напр. `C:\OpenFO-GitHub`.
2. Встанови **Git** і **GitHub CLI** кнопкою `INSTALL_GITHUB_TOOLS.cmd`, відкрий нове вікно термінала.
3. У PowerShell: `gh auth login --web` → вибери GitHub.com та HTTPS і підтвердь авторизацію в браузері.
4. Запусти `PUBLISH_TO_GITHUB.cmd`. Він створить **приватний** репозиторій
   `crvstaldreams/OpenFO-Windows-Build` (назву/власника бере з авторизованого акаунта),
   закомітить код та відправить його. Якщо назва вже зайнята — задай іншу:
   `powershell -ExecutionPolicy Bypass -File .\scripts\publish-to-github.ps1 -Repository OpenFO-Test-Build`
5. Відкрий вкладку **Actions → OpenFO Windows x64 — OpenGL + D3D9Ex → Run workflow**.
   Перша збірка також запускається після push у `main`, якщо Actions дозволені.
6. Після завершення відкрий build run → **Artifacts**. Знайдеш два архіви
   `OpenFO-baseline_084-Windows-x64` та `OpenFO-optimized_093-Windows-x64`.

Вони містять EXE та журнали. Для запуску гри додай **власні законно отримані** файли `data/`
у копію каталогу FlatOut. У репозиторій або CI такі ресурси **не завантажуються**.

## Діагностика збірки

- Якщо GitHub Actions впаде, відкрий логи job `build-windows` та вкладені `build.log` з artifacts (якщо файл створено).
- Порівнюй FPS між двома збірками в однаковій сцені, з однаковими налаштуваннями й VSync off.
- Для проблеми D3D9Ex важливо порівняти `PresentEx/ResetEx`, shader errors і `1% low`.
- Паралельні матричні job не змішують залежності та вихідні файли.
- `source/optimized_093` — реконструйована доступна кодова база OpenFO 0.9.3,
  **не гарантовано точне джерело поточного EXE**, який ти запускаєш на ПК.
- Робота Windows-збірки в GitHub Actions **ще не підтверджена**: потрібно отримати результат першого run.

## Структура

- `.github/workflows/windows-build.yml` — CI: 2 Windows x64 збірки, CTest, artifacts;
- `source/baseline_084` — вихідний код базової версії 0.8.4;
- `source/optimized_093` — вихідний код з оптимізаціями і камерами;
- `scripts/build-local.ps1` — MSVC/Zig, контроль помилок, логи, тести;
- `scripts/fetch-deps.ps1` — Bullet 3.25, meshoptimizer 0.25 і pinned stb_vorbis;
- `PUBLISH_TO_GITHUB.cmd` — одноразове створення приватного репозиторію та push;
- `.gitignore` — ігрові ресурси, згенеровані EXE, збірки та кеші виключені з Git.

Зверни увагу: GitHub Actions не збирає Linux Master Server 0.9.4 — це окремий проєкт.
