#ifndef TEXTURELIBRARY_H
#define TEXTURELIBRARY_H

#include <vector>
#include <array>
#include <random>
#include <string>

/**
 * @brief Генерация кристаллографических текстур для экспорта в ANSYS.
 *
 * Ключевые конвенции (проверены на реальном выводе generate_random_angles):
 *   - Матрица ориентации — ACTIVE (sample->crystal), как quat-путь в ansysWrapper
 *   - Выходные углы — ANSYS LOCAL: THXY(Z), THYZ(X'), THZX(Y''), т.е. Z-X-Y (3-1-2)
 *   - Bunge углы (phi1,Phi,phi2) — passive Z-X-Z; конвертер учитывает разницу
 *
 * Эталонные соответствия (юнит-тест в .cpp):
 *   Cube  {001}<100>  -> ANSYS (0,   0,     0)
 *   Goss  {110}<001>  -> ANSYS (0,   45,    0)
 *   Copper{112}<11-1> -> ANSYS (45,  0,     35.26)
 *   Brass {110}<112>  -> ANSYS (35,  45,    0)
 */
class TextureLibrary
{
public:
    // 3x3 матрица, строки — репер образца в координатах кристалла (active)
    using Matrix3 = std::array<std::array<double, 3>, 3>;

    // Одна идеальная компонента текстуры
    struct Component {
        int    hkl[3];        // плоскость {hkl} || плоскость листа (нормаль -> ND)
        int    uvw[3];        // направление <uvw> || RD  (для fiber-компонент — ось волокна)
        double scatter_deg;   // разброс вокруг идеала (гаусс), градусы
        double weight;        // вес в смеси (нормируется автоматически)
        std::string name;     // "Copper", "Goss", ...
        bool is_random = false;
        bool is_fiber  = false;   // true: uvw — ось волокна, вращение вокруг неё равновероятно
    };

    enum class Mode {
        Cube,       // все зёрна выровнены (нули) — бывший is_random=false
        Random,     // равномерно по SO(3)     — бывший is_random=true
        Textured    // смесь компонент + scatter
    };

    // Метод изготовления материала -> типовой набор идеальных компонент текстуры
    enum class Process {
        Extrusion,          // волоконная текстура <uvw>||ED
        Rolling,            // прокатка / вытяжка листа (Copper/S/Brass/Cube)
        Recrystallization,  // отжиг: Cube/Goss
        Shear,              // кручение / сдвиг: A,B,C
        Random              // чисто случайная ориентация зёрен (равномерно по SO(3))
    };

    explicit TextureLibrary(unsigned int seed = 0);

    // ── Настройка ──
    void setSeed(unsigned int seed);
    void setMode(Mode mode);
    void setComponents(const std::vector<Component>& comps);  // включает Mode::Textured
    void clear();

    // ── Поштучный сэмплинг (вызывается из createLocalCS) ──
    // Пишет THXY,THYZ,THZX в angl[3]. in_deg=true -> градусы.
    void sampleNext(double angl[3], bool in_deg = true);

    // ── Конвертер для UI (angle converter на правой панели) ──
    // Bunge (phi1,Phi,phi2) [deg] -> ANSYS THXY,THYZ,THZX [deg]
    static void bungeToAnsys(double phi1, double Phi, double phi2,
                             double& thxy, double& thyz, double& thzx);

    // {hkl}<uvw> -> Bunge (phi1,Phi,phi2) [deg]
    static void millerToBunge(const int hkl[3], const int uvw[3],
                              double& phi1, double& Phi, double& phi2);

    // ── Каталог пресетов ──
    static std::vector<Component> presetCatalog();

    // ── Типовые наборы компонент по методу изготовления ──
    static std::vector<Component> processComponents(Process p);
    static int         processCount() { return 5; }
    static std::string processName(Process p);
    static std::string processDesc(Process p);

    // ── Тесты (возвращает true, если все эталоны сошлись) ──
    static bool runSelfTest();

private:
    // Ядро математики
    static Matrix3 orientationFromMiller(const int hkl[3], const int uvw[3]);
    static Matrix3 orientationFromMillerActive(const int hkl[3], const int uvw[3]);
    static Matrix3 orientationFromFiberAxis(const int axis[3], double azimuth); // волоконная текстура
    static Matrix3 bungeToMatrix(double phi1, double Phi, double phi2);   // active
    static void    matrixToAnsys(const Matrix3& R,
                                 double& thxy, double& thyz, double& thzx,
                                 bool in_deg);
    static Matrix3 randomMatrix(std::mt19937& rng);
    static Matrix3 applyScatter(const Matrix3& ideal, double scatter_deg,
                                std::mt19937& rng);
    static Matrix3 matmul(const Matrix3& A, const Matrix3& B);

    const Component& pickComponent();   // взвешенный выбор

    Mode                    m_mode = Mode::Cube;
    std::vector<Component>  m_components;
    std::vector<double>     m_cumWeights;   // кумулятивные веса для выбора
    std::mt19937            m_rng;
};

#endif // TEXTURELIBRARY_H
