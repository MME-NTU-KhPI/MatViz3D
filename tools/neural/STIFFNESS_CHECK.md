# Проверка решателя жёсткости на любом компьютере

`stiffness_check.py` считает жёсткость ячейки «материал / пустота» решателем кнопки **Solve and compare** вкладки Neural (`gpu_stiffness.py`, должен лежать в той же папке). Расчёт идёт на видеокарте, если PyTorch её видит, иначе на процессоре.

Из всей MatViz3D на Python работает только `gpu_stiffness.py`. Генерация структур, проверка печатаемости, FFT-решатель на процессоре, которым посчитан датасет, и все три нейросети вкладки Neural написаны на C++. Если программа не находит Python с PyTorch и видеокартой, «Solve and compare» считает тем же методом на C++ на процессоре, только медленнее.

## Установка

Нужны Python 3.9 или новее, NumPy и PyTorch, для файлов `.hdf5` ещё h5py. Подходят видеокарты NVIDIA (`cuda`), Apple Silicon (`mps`) и Intel (`xpu`). Без них, в том числе с AMD на Windows, считает процессор, примерно в 10-15 раз медленнее.

Windows с NVIDIA:

1. Поставить Python с python.org, отметив «Add python.exe to PATH».
2. `py -m pip install numpy h5py`
3. Поставить PyTorch со сборкой для CUDA: команду даёт pytorch.org, раздел «Get Started» (Windows, Pip, CUDA). Обычный `pip install torch` на Windows ставит сборку без CUDA.
4. Проверить: `py -c "import torch; print(torch.cuda.is_available())"` должно напечатать `True`.

Mac: `python3 -m pip install torch numpy h5py`.

## Запуск

На Windows `py`, на Mac и Linux `python3`:

| Команда | Что делает |
|---|---|
| `py stiffness_check.py` | самопроверка, около минуты |
| `py stiffness_check.py cell.hdf5` | ячейка из файла MatViz3D или датасета; сравнение с ответом, записанным в файле |
| `py stiffness_check.py folder` | все файлы `.hdf5`, `.h5`, `.npy` папки и общая таблица |
| `py stiffness_check.py cell.npy` | массив n × n × n, ненулевое значение = материал |
| `py stiffness_check.py cell.raw --n 64` | n³ байт в порядке, в каком программа передаёт ячейку решателю |

Ключи: `--device cuda|mps|xpu|cpu` (где считать), `--compare_cpu` (повторить на процессоре и сравнить), `--e_gpa`, `--nu`, `--aniso` (материал; по умолчанию из файла, иначе 2 ГПа, 0.38, 1), `--json results.json` (сохранить результаты).

## Самопроверка

| Ячейка | Чем проверяется |
|---|---|
| `solid`, сплошной куб | ответ совпадает с матрицей материала |
| `layers_x`, `layers_y`, `layers_z`, плоские слои | ответ совпадает с точной формулой для слоёв; материал мягче вдоль z, поэтому перепутанные оси дают расхождение |
| `gyroid` | кубическая симметрия: Ex = Ey = Ez, три модуля сдвига равны |
| `gyroid` 32³ | видеокарта и процессор дают один ответ |

В конце должно быть `all 6 checks passed`. У слоёв доля энергии в материале около 0 и второй проход: поперёк слоёв материал нагрузку не несёт, так и должно быть.

## Примеры с выводом

Каждый блок начинается с команды (`$` обозначает приглашение терминала), дальше идёт её вывод без изменений. Выводы получены на Mac с видеокартой Apple (`mps`) командами из папки со скриптом; ячейки `000004.hdf5` и `000066.hdf5` лежат там в папке `examples`. Видеокарту в это время делило обучение нейросети, поэтому время завышено. На компьютере с NVIDIA вместо `mps` будет `cuda` с названием видеокарты; модули и расхождения должны совпасть до последних знаков, время и число итераций могут немного отличаться. Строки, начинающиеся с `...`, показывают ход расчёта раз в 5 секунд и от запуска к запуску разные; в примерах 3 и 4 они опущены.

### 1. Самопроверка

```
$ python3 stiffness_check.py
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

self-test on 64^3 cells: material nu 0.38, aniso 0.8 (stiffness across z is 0.8 of that in the plane), gyroid with aniso 1

1. solid: solid share 1.000
   mps: 0 iterations, converged, void 1e-4, solid share 1.000, 1.9 s, GPU memory 1.17 GB
   answer against the material matrix, largest difference / C11: 1.48e-07 (limit 1e-04) -> PASS

2. layers_x: solid share 0.500
   mps: 10 iterations, converged, second pass with void 1e-6 (solid held 0.000 < 0.985 of the energy with 1e-4), solid share 0.000, 4.1 s, GPU memory 1.17 GB
   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)
   answer against the exact layer formula, largest difference / C11: 6.78e-08 (limit 1e-04) -> PASS
   moduli / E here:  Ex 0.00000 Ey 0.50000 Ez 0.40000 Gxy 0.00000 Gyz 0.14493 Gxz 0.00000
   moduli / E exact: Ex 0.00000 Ey 0.50000 Ez 0.40000 Gxy 0.00000 Gyz 0.14493 Gxz 0.00000

3. layers_y: solid share 0.500
   mps: 15 iterations, converged, second pass with void 1e-6 (solid held 0.000 < 0.985 of the energy with 1e-4), solid share 0.000, 3.9 s, GPU memory 1.17 GB
   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)
   answer against the exact layer formula, largest difference / C11: 6.78e-08 (limit 1e-04) -> PASS
   moduli / E here:  Ex 0.50000 Ey 0.00000 Ez 0.40000 Gxy 0.00000 Gyz 0.00000 Gxz 0.14493
   moduli / E exact: Ex 0.50000 Ey 0.00000 Ez 0.40000 Gxy 0.00000 Gyz 0.00000 Gxz 0.14493

4. layers_z: solid share 0.500
   mps: 24 iterations, converged, second pass with void 1e-6 (solid held 0.000 < 0.985 of the energy with 1e-4), solid share 0.000, 5.0 s, GPU memory 1.17 GB
   (across the layers nothing but the void carries a load, so a solid share near 0, the second pass and moduli near 0 across are the right answer)
   answer against the exact layer formula, largest difference / C11: 1.08e-07 (limit 1e-04) -> PASS
   moduli / E here:  Ex 0.50000 Ey 0.50000 Ez 0.00000 Gxy 0.18116 Gyz 0.00000 Gxz 0.00000
   moduli / E exact: Ex 0.50000 Ey 0.50000 Ez 0.00000 Gxy 0.18116 Gyz 0.00000 Gxz 0.00000

5. gyroid: solid share 0.293
      ... 90 iterations, residual 3.1e-04, loads done 3 of 6
   mps: 735 iterations, converged, void 1e-4, solid share 0.998, 10.0 s, GPU memory 1.17 GB
   moduli / E: Ex 0.10613 Ey 0.10613 Ez 0.10613 Gxy 0.04555 Gyz 0.04555 Gxz 0.04555
   cubic symmetry: spread of Ex, Ey, Ez and of the three G, relative: 4.91e-05 (limit 5e-04) -> PASS

6. gyroid 32^3 on mps and on the CPU (the CPU is slower; a smaller cell)
      ... 30 iterations, residual 1.3e-02, loads done 0 of 6
      ... 70 iterations, residual 4.6e-04, loads done 0 of 6
      ... 100 iterations, residual 8.3e-04, loads done 3 of 6
      ... 130 iterations, residual 8.8e-05, loads done 3 of 6
      ... 160 iterations, residual 1.6e-04, loads done 3 of 6
   mps: 815 iterations, converged, void 1e-4, solid share 0.996, 1.9 s, GPU memory 1.17 GB
   cpu: 813 iterations, converged, void 1e-4, solid share 0.996, 35.2 s
   moduli on the GPU against the CPU, largest relative difference: 1.18e-04 (limit 1e-03) -> PASS

all 6 checks passed on mps
```

### 2. Одна ячейка из файла датасета

Ячейка 000004: материал 2.568 ГПа, ν 0.4305, поперёк слоёв печати 0.811 от жёсткости вдоль них; всё это скрипт берёт из файла.

```
$ python3 stiffness_check.py examples/000004.hdf5
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

000004.hdf5 (group 1): 64^3, solid share of the voxels 0.468, material E 2.568 GPa, nu 0.4305, aniso 0.811 (from the file)
      ... 90 iterations, residual 8.1e-04, loads done 0 of 6
      ... 190 iterations, residual 1.8e-04, loads done 0 of 6
      ... 290 iterations, residual 3.2e-05, loads done 0 of 6
   mps: 1913 iterations, converged, void 1e-4, solid share 0.993, 18.6 s, GPU memory 1.17 GB
   moduli, GPa:  Ex 0.42945  Ey 0.32558  Ez 0.30963  Gxy 0.13711  Gyz 0.11986  Gxz 0.14314
   relative to E: Ex 0.16723  Ey 0.12678  Ez 0.12057  Gxy 0.05339  Gyz 0.04667  Gxz 0.05574
   stiffness matrix C, GPa (order xx yy zz xy yz xz, engineering shear):
                   xx         yy         zz         xy         yz         xz
        xx    0.57624    0.13882    0.17651    0.04483    0.00632    0.12677
        yy    0.13882    0.38392    0.12619    0.02908    0.01039    0.02594
        zz    0.17651    0.12619    0.41734    0.01044    0.00743    0.11110
        xy    0.04483    0.02908    0.01044    0.14833    0.02877    0.00272
        yz    0.00632    0.01039    0.00743    0.02877    0.12704    0.01542
        xz    0.12677    0.02594    0.11110    0.00272    0.01542    0.18869
   stored answer of the file: E differ by 0.0012 %, G by 0.0111 %, the matrix by 0.0039 % -> PASS (limit 0.1 %)

everything passed
```

### 3. Все ячейки папки

Ячейка 000066 считается дважды: с пустотой 1e-4 материал несёт 0.983 энергии, меньше порога 0.985, поэтому второй проход идёт с пустотой 1e-6, как в датасете.

```
$ python3 stiffness_check.py examples
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

(блок ячейки 000004.hdf5, как в примере 2)

000066.hdf5 (group 1): 64^3, solid share of the voxels 0.400, material E 2.115 GPa, nu 0.4465, aniso 0.672 (from the file)
   mps: 6335 iterations, converged, second pass with void 1e-6 (solid held 0.983 < 0.985 of the energy with 1e-4), solid share 1.000, 95.7 s, GPU memory 1.17 GB
   moduli, GPa:  Ex 0.36720  Ey 0.05530  Ez 0.18655  Gxy 0.07954  Gyz 0.06103  Gxz 0.11391
   relative to E: Ex 0.17362  Ey 0.02615  Ez 0.08820  Gxy 0.03761  Gyz 0.02885  Gxz 0.05386
   stiffness matrix C, GPa (order xx yy zz xy yz xz, engineering shear):
                   xx         yy         zz         xy         yz         xz
        xx    0.47292    0.08205    0.14912   -0.00456   -0.00797    0.05021
        yy    0.08205    0.08975    0.09679    0.00296   -0.00298    0.00125
        zz    0.14912    0.09679    0.34499   -0.01280   -0.06027    0.03337
        xy   -0.00456    0.00296   -0.01280    0.08461    0.00893   -0.02298
        yz   -0.00797   -0.00298   -0.06027    0.00893    0.07529   -0.01102
        xz    0.05021    0.00125    0.03337   -0.02298   -0.01102    0.12779
   stored answer of the file: E differ by 0.0005 %, G by 0.0005 %, the matrix by 0.0005 % -> PASS (limit 0.1 %)

cell                                 grid  seconds   Ex / E   vs file  result
000004.hdf5 (group 1)                64^3     24.7   0.1672  0.0012 %  PASS
000066.hdf5 (group 1)                64^3     95.7   0.1736  0.0005 %  PASS

everything passed
```

### 4. Видеокарта против процессора

```
$ python3 stiffness_check.py gyroid --n 32 --compare_cpu
Python 3.12.7 (/opt/anaconda3/bin/python3), PyTorch 2.12.0, NumPy 1.26.4, Darwin arm64
devices PyTorch sees: mps, cpu
solving on mps: Apple GPU (Metal), arm64

gyroid (built in): 32^3, solid share of the voxels 0.285, material E 2 GPa, nu 0.38, aniso 1 (default)
   mps: 815 iterations, converged, void 1e-4, solid share 0.996, 1.8 s, GPU memory 0.10 GB
   moduli, GPa:  Ex 0.19515  Ey 0.19516  Ez 0.19515  Gxy 0.07917  Gyz 0.07918  Gxz 0.07917
   relative to E: Ex 0.09758  Ey 0.09758  Ez 0.09758  Gxy 0.03959  Gyz 0.03959  Gxz 0.03959
   stiffness matrix C, GPa (order xx yy zz xy yz xz, engineering shear):
                   xx         yy         zz         xy         yz         xz
        xx    0.28563    0.13853    0.13853   -0.00000    0.00000   -0.00001
        yy    0.13853    0.28565    0.13854    0.00000    0.00000   -0.00000
        zz    0.13853    0.13854    0.28564    0.00000    0.00000   -0.00000
        xy   -0.00000    0.00000    0.00000    0.07917    0.00000    0.00000
        yz    0.00000    0.00000    0.00000    0.00000    0.07918    0.00000
        xz   -0.00001   -0.00000   -0.00000    0.00000    0.00000    0.07917
   the same on the CPU ...
   cpu: 813 iterations, converged, void 1e-4, solid share 0.996, 29.8 s
   GPU against CPU: moduli differ by 0.0118 % at most, the GPU is 16.4 times faster -> PASS

everything passed
```

## Как читать вывод

- `converged`: расчёт сошёлся. При `NOT CONVERGED` ответу верить нельзя.
- `second pass with void 1e-6`: ячейка пересчитана с более мягкой пустотой, по тому же правилу, что и весь датасет.
- `solid share` ниже 0.9: у ячейки нет пути нагрузки хотя бы в одном направлении.
- Модули Ex, Ey, Ez, Gxy, Gyz, Gxz и матрица C даются в ГПа, порядок xx, yy, zz, xy, yz, xz.
- `stored answer of the file ... PASS`: расхождение с ответом, записанным в файле датасета, не больше 0.1 % (обычно 0.001-0.01 %).

Код выхода 0 означает, что всё прошло.

## Частые проблемы

| Сообщение | Что делать |
|---|---|
| `CUDA: this PyTorch is a CPU-only build` | поставить PyTorch со сборкой для CUDA (Установка, шаг 3) |
| `PyTorch is built for CUDA ... but sees no card` | обновить драйвер NVIDIA |
| `reading .hdf5 needs h5py` | `py -m pip install h5py` |
| `out of memory` | ячейке 128³ нужно около 5 ГБ видеопамяти, 64³ около 1 ГБ; взять `--device cpu` |

## MatViz3D на Windows

Программа ищет Python только по путям macOS и Linux. На Windows ей нужно указать Python, на котором прошла самопроверка, и перезапустить её:

```
setx MATVIZ_GPU_PYTHON "C:\Users\<имя>\AppData\Local\Programs\Python\Python312\python.exe"
```
