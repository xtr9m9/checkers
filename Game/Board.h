#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL.h>
    #include <SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    Board() = default; // Конструктор по умолчанию

    // Конструктор, инициализирующий ширину (W) и высоту (H) доски
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // Метод для отрисовки начальной доски
    int start_draw()
    {
        // Инициализация SDL2
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1; // Ошибка инициализации SDL
        }

        // Если ширина и высота не заданы, устанавливаем их в зависимости от экрана
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desktop display mode");
                return 1;
            }

            // Выбираем минимальное значение между шириной и высотой экрана, чтобы доска была квадратной
            W = min(dm.w, dm.h);
            W -= W / 15; // Немного уменьшаем размер, чтобы оставить отступы
            H = W;
        }

        // Создаем окно игры "Checkers"
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1; // Ошибка создания окна
        }

        // Создаем рендерер для отрисовки графики
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1; // Ошибка создания рендерера
        }

        // Загружаем текстуры для доски, шашек и кнопок
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());

        // Проверяем, удалось ли загрузить все текстуры
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1; // Ошибка загрузки текстур
        }

        // Получаем окончательный размер окна
        SDL_GetRendererOutputSize(ren, &W, &H);

        // Создаем начальную матрицу игрового поля
        make_start_mtx();

        // Перерисовываем игровую сцену
        rerender();

        return 0; // Успешный запуск
    }

    void redraw()
    {
        game_results = -1; // Сбрасываем результаты игры (например, проигрыш или победа)
        history_mtx.clear(); // Очищаем историю состояния доски
        history_beat_series.clear(); // Очищаем историю серий побежденных фигур
        make_start_mtx(); // Пересоздаем начальное состояние доски
        clear_active(); // Убираем выделение активной клетки
        clear_highlight(); // Убираем все выделенные клетки
    }

    void move_piece(move_pos turn, const int beat_series = 0)
    {
        // Если фигура была побита (в начале хранящаяся в xb, yb), то ее нужно убрать с доски
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0; // Убираем побитую фигуру с позиции
        }

        // Перемещаем фигуру с одной клетки на другую (используя другую версию функции)
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Проверяем, пуста ли целевая клетка
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }

        // Проверяем, есть ли фигура на начальной клетке
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }

        // Если фигура дошла до последнего ряда (в зависимости от ее цвета), она становится дамкой
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2; // Превращаем в дамку (прибавляем 2 к значению фигуры)

        // Перемещаем фигуру на целевую позицию
        mtx[i2][j2] = mtx[i][j];
        drop_piece(i, j); // Убираем фигуру с начальной клетки
        add_history(beat_series); // Добавляем ход в историю (с учетом серии побежденных фигур)
    }

    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0; // Убираем фигуру с клетки (ставим 0, означающее пустую клетку)
        rerender(); // Перерисовываем доску, чтобы изменения отобразились на экране
    }

    void turn_into_queen(const POS_T i, const POS_T j)
    {
        // Проверка, что фигура существует и она не является дамкой
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position"); // Выбрасываем исключение, если клетка пуста или уже дамка
        }
        mtx[i][j] += 2; // Превращаем фигуру в дамку, прибавляя 2 (например, 1 -> 3 для белой дамки, 2 -> 4 для черной дамки)
        rerender(); // Перерисовываем доску, чтобы изменения отобразились на экране
    }
    vector<vector<POS_T>> get_board() const
    {
        return mtx; // Возвращаем текущее состояние доски (матрицу клеток)
    }

    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        // Проходим по всем переданным клеткам и помечаем их как подсвеченные
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1; // Устанавливаем флаг подсветки для каждой клетки
        }
        rerender(); // Перерисовываем доску, чтобы подсветка отобразилась на экране
    }

    void clear_highlight()
    {
        // Очищаем все клетки от подсветки (устанавливаем флаг подсветки в 0)
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0); // Присваиваем каждой клетке значение 0 (не подсвечивается)
        }
        rerender(); // Перерисовываем доску, чтобы очистить подсветку
    }

    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x; // Устанавливаем активные координаты клетки
        active_y = y;
        rerender(); // Перерисовываем доску, чтобы активная клетка отображалась
    }

    void clear_active()
    {
        active_x = -1; // Сбрасываем координаты активной клетки (устанавливаем их в -1, чтобы показать, что нет активной клетки)
        active_y = -1;
        rerender(); // Перерисовываем доску, чтобы удалить выделение активной клетки
    }

    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y]; // Возвращает, подсвечена ли данная клетка
    }

    void rollback()
    {
        // Получаем количество ударов (битых фигур) для отката
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        // Откатываем состояние доски, удаляя элементы из истории
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back(); // Удаляем последний ход из истории
            history_beat_series.pop_back(); // Удаляем количество ударов, связанное с этим ходом
        }
        mtx = *(history_mtx.rbegin()); // Восстанавливаем доску из последнего состояния истории
        clear_highlight(); // Очищаем подсветку
        clear_active(); // Очищаем активную клетку
    }

    void show_final(const int res)
    {
        game_results = res; // Сохраняем результат игры (результат может быть: победа, ничья или проигрыш)
        rerender(); // Перерисовываем доску, чтобы отобразить результат игры на экране
    }

    // use if window size changed
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H); // Получаем новый размер окна после изменения
        rerender(); // Перерисовываем доску с учетом нового размера окна
    }

    void quit()
    {
        // Освобождаем ресурсы, связанные с графическими объектами и окном
        SDL_DestroyTexture(board); // Удаляем текстуру для фона доски
        SDL_DestroyTexture(w_piece); // Удаляем текстуру для белых фигур
        SDL_DestroyTexture(b_piece); // Удаляем текстуру для черных фигур
        SDL_DestroyTexture(w_queen); // Удаляем текстуру для белых дам
        SDL_DestroyTexture(b_queen); // Удаляем текстуру для черных дам
        SDL_DestroyTexture(back); // Удаляем текстуру фона
        SDL_DestroyTexture(replay); // Удаляем текстуру кнопки для воспроизведения
        SDL_DestroyRenderer(ren); // Удаляем рендерер
        SDL_DestroyWindow(win); // Удаляем окно
        SDL_Quit(); // Завершаем работу с SDL
    }

    ~Board()
    {
        if (win) // Если окно существует
            quit(); // Вызываем функцию для освобождения ресурсов перед завершением работы программы
    }

private:
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }
    // function to make start matrix
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;
            }
        }
        add_history();
    }

    // function that re-draw all the textures
    void rerender()
    {
        // draw board
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // draw pieces
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j])
                    continue;
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // draw hilight
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // draw active
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);

        // draw arrows
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // draw result
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path;
            else if (game_results == 2)
                result_path = black_path;
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        SDL_RenderPresent(ren);
        // next rows for mac os
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;
        fout.close();
    }

  public:
    int W = 0;
    int H = 0;
    // history of boards
    vector<vector<vector<POS_T>>> history_mtx;

  private:
    SDL_Window *win = nullptr;
    SDL_Renderer *ren = nullptr;
    // textures
    SDL_Texture *board = nullptr;
    SDL_Texture *w_piece = nullptr;
    SDL_Texture *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr;
    SDL_Texture *b_queen = nullptr;
    SDL_Texture *back = nullptr;
    SDL_Texture *replay = nullptr;
    // texture files names
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";
    // coordinates of chosen cell
    int active_x = -1, active_y = -1;
    // game result if exist
    int game_results = -1;
    // matrix of possible moves
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    // matrix of possible moves
    // 1 - white, 2 - black, 3 - white queen, 4 - black queen
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    // series of beats for each move
    vector<int> history_beat_series;
};
