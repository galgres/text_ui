#include <windows.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kTitle[] = L"Title";

constexpr WORD kWhite = FOREGROUND_RED | FOREGROUND_GREEN |
                        FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kYellow = FOREGROUND_RED | FOREGROUND_GREEN |
                         FOREGROUND_INTENSITY;
constexpr WORD kBlue = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD kRedBackground = BACKGROUND_RED | BACKGROUND_INTENSITY;
constexpr WORD kPurpleBackground = BACKGROUND_RED | BACKGROUND_BLUE |
                                   BACKGROUND_INTENSITY;
constexpr WORD kGrayBackground = BACKGROUND_RED | BACKGROUND_GREEN |
                                 BACKGROUND_BLUE;

struct Rectangle {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

class ConsoleSession {
public:
    ConsoleSession()
        : input_(GetStdHandle(STD_INPUT_HANDLE)),
          originalOutput_(GetStdHandle(STD_OUTPUT_HANDLE)) {
        if (input_ == INVALID_HANDLE_VALUE || originalOutput_ == INVALID_HANDLE_VALUE ||
            !GetConsoleMode(input_, &originalInputMode_)) {
            return;
        }

        wchar_t title[1024]{};
        const DWORD titleLength = GetConsoleTitleW(title, 1024);
        originalTitle_.assign(title, titleLength);

        output_ = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr);
        if (output_ == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD inputMode = originalInputMode_;
        inputMode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT;
        inputMode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

        if (!SetConsoleMode(input_, inputMode) ||
            !SetConsoleActiveScreenBuffer(output_)) {
            CloseHandle(output_);
            output_ = INVALID_HANDLE_VALUE;
            return;
        }

        CONSOLE_CURSOR_INFO cursorInfo{};
        cursorInfo.dwSize = 1;
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(output_, &cursorInfo);
        SetConsoleTitleW(kTitle);
        valid_ = true;
    }

    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;

    ~ConsoleSession() {
        if (output_ != INVALID_HANDLE_VALUE) {
            SetConsoleActiveScreenBuffer(originalOutput_);
            CloseHandle(output_);
        }
        if (input_ != INVALID_HANDLE_VALUE) {
            SetConsoleMode(input_, originalInputMode_);
        }
        if (!originalTitle_.empty()) {
            SetConsoleTitleW(originalTitle_.c_str());
        }
    }

    [[nodiscard]] bool IsValid() const { return valid_; }
    [[nodiscard]] HANDLE Input() const { return input_; }
    [[nodiscard]] HANDLE Output() const { return output_; }

private:
    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE originalOutput_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    DWORD originalInputMode_ = 0;
    std::wstring originalTitle_;
    bool valid_ = false;
};

class TerminalUi {
public:
    explicit TerminalUi(HANDLE output) : output_(output) {}

    bool Draw() {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output_, &info)) {
            return false;
        }

        viewport_ = info.srWindow;
        width_ = viewport_.Right - viewport_.Left + 1;
        height_ = viewport_.Bottom - viewport_.Top + 1;
        if (width_ <= 0 || height_ <= 0) {
            return false;
        }

        const WORD background = purple_ ? kPurpleBackground : kRedBackground;
        cells_.assign(static_cast<std::size_t>(width_ * height_),
                      MakeCell(L' ', background | kWhite));

        DrawFrame(background);
        CalculateButtons();
        DrawButton(changeButton_, L"Change color", focus_ == 0, background);
        DrawButton(exitButton_, L"Exit", focus_ == 1, background);

        const std::wstring help = L"Tab / arrows: focus   Enter: select   Esc: exit";
        PutCenteredText(height_ - 2, help, background | kWhite);

        COORD dataSize{static_cast<SHORT>(width_), static_cast<SHORT>(height_)};
        COORD dataOrigin{0, 0};
        SMALL_RECT target = viewport_;
        return WriteConsoleOutputW(output_, cells_.data(), dataSize,
                                   dataOrigin, &target) != FALSE;
    }

    void MoveFocus() {
        focus_ = focus_ == 0 ? 1 : 0;
    }

    // Returns false when the selected action requests application exit.
    bool ActivateFocusedButton() {
        if (focus_ == 0) {
            purple_ = !purple_;
            return true;
        }
        return false;
    }

private:
    static CHAR_INFO MakeCell(wchar_t character, WORD attributes) {
        CHAR_INFO cell{};
        cell.Char.UnicodeChar = character;
        cell.Attributes = attributes;
        return cell;
    }

    void PutCell(int x, int y, wchar_t character, WORD attributes) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            return;
        }
        cells_[static_cast<std::size_t>(y * width_ + x)] =
            MakeCell(character, attributes);
    }

    void PutText(int x, int y, const std::wstring& text, WORD attributes) {
        for (std::size_t index = 0; index < text.size(); ++index) {
            PutCell(x + static_cast<int>(index), y, text[index], attributes);
        }
    }

    void PutCenteredText(int y, const std::wstring& text, WORD attributes) {
        const int x = std::max(1, (width_ - static_cast<int>(text.size())) / 2);
        PutText(x, y, text, attributes);
    }

    void DrawFrame(WORD background) {
        if (width_ < 2 || height_ < 2) {
            return;
        }

        const WORD frameAttributes = background | kWhite;
        for (int x = 1; x < width_ - 1; ++x) {
            PutCell(x, 0, L'-', frameAttributes);
            PutCell(x, height_ - 1, L'-', frameAttributes);
        }
        for (int y = 1; y < height_ - 1; ++y) {
            PutCell(0, y, L'|', frameAttributes);
            PutCell(width_ - 1, y, L'|', frameAttributes);
        }
        PutCell(0, 0, L'+', frameAttributes);
        PutCell(width_ - 1, 0, L'+', frameAttributes);
        PutCell(0, height_ - 1, L'+', frameAttributes);
        PutCell(width_ - 1, height_ - 1, L'+', frameAttributes);

        const std::wstring caption = L" Title ";
        PutCenteredText(0, caption, frameAttributes);
    }

    void CalculateButtons() {
        constexpr int changeWidth = 20;
        constexpr int exitWidth = 12;
        constexpr int buttonHeight = 3;
        constexpr int gap = 3;

        if (width_ >= changeWidth + exitWidth + gap + 4) {
            const int totalWidth = changeWidth + gap + exitWidth;
            const int left = (width_ - totalWidth) / 2;
            const int top = std::max(2, height_ / 2 - 1);
            changeButton_ = {left, top, left + changeWidth - 1,
                             top + buttonHeight - 1};
            exitButton_ = {left + changeWidth + gap, top,
                           left + totalWidth - 1, top + buttonHeight - 1};
        } else {
            const int left = std::max(1, (width_ - changeWidth) / 2);
            const int top = std::max(2, height_ / 2 - 3);
            changeButton_ = {left, top, left + changeWidth - 1,
                             top + buttonHeight - 1};
            const int exitLeft = std::max(1, (width_ - exitWidth) / 2);
            exitButton_ = {exitLeft, top + buttonHeight + 1,
                           exitLeft + exitWidth - 1,
                           top + buttonHeight * 2};
        }
    }

    void DrawButton(const Rectangle& button, const std::wstring& label,
                    bool focused, WORD windowBackground) {
        const WORD interior = kGrayBackground;
        const WORD border = kGrayBackground | (focused ? kBlue : 0);

        for (int y = button.top; y <= button.bottom; ++y) {
            for (int x = button.left; x <= button.right; ++x) {
                PutCell(x, y, L' ', interior);
            }
        }

        for (int x = button.left + 1; x < button.right; ++x) {
            PutCell(x, button.top, L'-', border);
            PutCell(x, button.bottom, L'-', border);
        }
        PutCell(button.left, button.top, L'+', border);
        PutCell(button.right, button.top, L'+', border);
        PutCell(button.left, button.bottom, L'+', border);
        PutCell(button.right, button.bottom, L'+', border);
        PutCell(button.left, button.top + 1, L'|', border);
        PutCell(button.right, button.top + 1, L'|', border);

        const int labelX = button.left +
                           (button.right - button.left + 1 -
                            static_cast<int>(label.size())) / 2;
        PutText(labelX, button.top + 1, label, interior);

        if (focused) {
            PutCell(button.left - 2, button.top + 1, L'>',
                    windowBackground | kYellow);
            PutCell(button.right + 2, button.top + 1, L'<',
                    windowBackground | kYellow);
        }
    }

    HANDLE output_;
    SMALL_RECT viewport_{};
    int width_ = 0;
    int height_ = 0;
    int focus_ = 0;
    bool purple_ = false;
    Rectangle changeButton_{};
    Rectangle exitButton_{};
    std::vector<CHAR_INFO> cells_;
};

} // namespace

int main() {
    ConsoleSession console;
    if (!console.IsValid()) {
        std::wcerr << L"This application requires a Windows console.\n";
        return 1;
    }

    TerminalUi ui(console.Output());
    if (!ui.Draw()) {
        return 1;
    }

    bool running = true;
    while (running) {
        INPUT_RECORD event{};
        DWORD eventsRead = 0;
        if (!ReadConsoleInputW(console.Input(), &event, 1, &eventsRead)) {
            return 1;
        }

        bool redraw = false;
        if (event.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            redraw = true;
        } else if (event.EventType == KEY_EVENT &&
                   event.Event.KeyEvent.bKeyDown) {
            switch (event.Event.KeyEvent.wVirtualKeyCode) {
            case VK_TAB:
            case VK_LEFT:
            case VK_RIGHT:
                ui.MoveFocus();
                redraw = true;
                break;
            case VK_RETURN:
            case VK_SPACE:
                running = ui.ActivateFocusedButton();
                redraw = running;
                break;
            case VK_ESCAPE:
                running = false;
                break;
            default:
                break;
            }
        }

        if (redraw && !ui.Draw()) {
            return 1;
        }
    }

    return 0;
}
