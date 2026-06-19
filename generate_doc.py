#!/usr/bin/env python3
"""Generates EMILI-IMRT documentation PDF."""
from fpdf import FPDF
import math

# --- colour palette ---------------------------------------------------------
PRIMARY   = (26,  82, 148)   # dark blue
SECONDARY = (41, 128, 185)   # medium blue
ACCENT    = (22, 160, 133)   # teal
LIGHT_BG  = (235, 245, 255)  # very light blue
ALT_BG    = (240, 255, 250)  # very light teal
WHITE     = (255, 255, 255)
BLACK     = (30,  30,  30)
GRAY      = (120, 120, 120)
GRAY_LIGHT= (200, 210, 220)
RED_SOFT  = (192,  57,  43)


class DocPDF(FPDF):
    # -- helpers --------------------------------------------------------------
    def set_rgb(self, color, draw=False):
        if draw:
            self.set_draw_color(*color)
        else:
            self.set_fill_color(*color)

    # -- header / footer ------------------------------------------------------
    def header(self):
        if self.page_no() == 1:
            return
        self.set_fill_color(*PRIMARY)
        self.rect(0, 0, 210, 12, 'F')
        self.set_text_color(*WHITE)
        self.set_font('Helvetica', 'B', 9)
        self.set_y(3)
        self.cell(0, 6, 'EMILI-IMRT: Documentación Técnica', align='C')
        self.set_text_color(*BLACK)
        self.ln(8)

    def footer(self):
        if self.page_no() == 1:
            return
        self.set_y(-14)
        self.set_fill_color(*PRIMARY)
        self.rect(0, self.get_y(), 210, 14, 'F')
        self.set_text_color(*WHITE)
        self.set_font('Helvetica', '', 8)
        self.cell(0, 10, f'Página {self.page_no()}', align='C')
        self.set_text_color(*BLACK)

    # -- cover -----------------------------------------------------------------
    def cover_page(self):
        self.add_page()
        # background gradient-like strip
        self.set_fill_color(*PRIMARY)
        self.rect(0, 0, 210, 80, 'F')
        self.set_fill_color(*SECONDARY)
        self.rect(0, 80, 210, 40, 'F')
        self.set_fill_color(*LIGHT_BG)
        self.rect(0, 120, 210, 177, 'F')

        # title block
        self.set_text_color(*WHITE)
        self.set_font('Helvetica', 'B', 28)
        self.set_y(18)
        self.cell(0, 14, 'EMILI-IMRT', align='C', ln=True)
        self.set_font('Helvetica', 'B', 16)
        self.cell(0, 10, 'Documentación Técnica Completa', align='C', ln=True)
        self.set_font('Helvetica', '', 11)
        self.cell(0, 8, 'Optimización de Radioterapia de Intensidad Modulada', align='C', ln=True)
        self.ln(2)
        self.set_font('Helvetica', 'I', 10)
        self.cell(0, 7, 'Framework EMILI + Subproblemas BAO y FMO', align='C', ln=True)

        # divider line
        self.set_draw_color(*WHITE)
        self.set_line_width(0.8)
        self.line(30, 68, 180, 68)

        # subtitle on secondary band
        self.set_y(84)
        self.set_font('Helvetica', 'B', 13)
        self.cell(0, 8, 'Beam Angle Optimization  ·  Fluence Map Optimization', align='C', ln=True)
        self.set_font('Helvetica', '', 10)
        self.cell(0, 6, 'Metaheurísticas · OSQP · Estructuras de Vecindad · Gramáticas de Configuración', align='C', ln=True)

        # info box
        self.set_y(130)
        self.set_fill_color(*WHITE)
        self.set_draw_color(*SECONDARY)
        self.set_line_width(0.5)
        self.rect(25, 128, 160, 48, 'FD')
        self.set_text_color(*PRIMARY)
        self.set_font('Helvetica', 'B', 11)
        self.set_y(133)
        self.cell(0, 7, 'Resumen del Sistema', align='C', ln=True)
        self.set_draw_color(*GRAY_LIGHT)
        self.line(40, self.get_y(), 170, self.get_y())
        self.ln(3)
        items = [
            ('Plataforma',     'C++ 11/14, CMake, OSQP'),
            ('Paradigma',      'Metaheurísticas + QP Exacto'),
            ('Dominio',        'Planificación de Tratamiento de RT'),
            ('Configuración',  'Gramáticas XML + irace/GGA'),
        ]
        self.set_text_color(*BLACK)
        for label, val in items:
            self.set_font('Helvetica', 'B', 9)
            self.set_x(35)
            self.cell(42, 6, label + ':', ln=False)
            self.set_font('Helvetica', '', 9)
            self.cell(0, 6, val, ln=True)

        self.set_text_color(*BLACK)

    # -- section heading -------------------------------------------------------
    def section(self, number, title, color=PRIMARY):
        self.ln(4)
        self.set_fill_color(*color)
        self.set_text_color(*WHITE)
        self.set_font('Helvetica', 'B', 13)
        self.cell(0, 9, f'  {number}  {title}', fill=True, ln=True)
        self.set_text_color(*BLACK)
        self.ln(2)

    def subsection(self, title, color=SECONDARY):
        self.ln(3)
        self.set_fill_color(*color)
        self.set_text_color(*WHITE)
        self.set_font('Helvetica', 'B', 10)
        self.cell(0, 7, f'  {title}', fill=True, ln=True)
        self.set_text_color(*BLACK)
        self.ln(1)

    def subsubsection(self, title):
        self.ln(2)
        self.set_draw_color(*ACCENT)
        self.set_line_width(0.6)
        self.set_fill_color(*ALT_BG)
        self.set_text_color(*ACCENT)
        self.set_font('Helvetica', 'B', 9)
        self.cell(0, 6, f'  >> {title}', fill=True, ln=True)
        self.set_text_color(*BLACK)
        self.set_line_width(0.2)

    # -- body text -------------------------------------------------------------
    def body(self, text, indent=0):
        self.set_font('Helvetica', '', 9)
        self.set_text_color(*BLACK)
        effective_w = 180 - indent
        if indent:
            self.set_x(15 + indent)
        self.multi_cell(effective_w, 5, text)
        self.ln(1)

    def bullet(self, text, level=1):
        bullet_char = '-' if level == 1 else '*'
        indent = 8 * level
        self.set_font('Helvetica', '', 9)
        self.set_text_color(*BLACK)
        self.set_x(15 + indent)
        self.cell(5, 5, bullet_char, ln=False)
        self.multi_cell(165 - indent, 5, text)

    def code_block(self, lines, title=''):
        if title:
            self.set_font('Helvetica', 'B', 8)
            self.set_text_color(*SECONDARY)
            self.set_x(15)
            self.cell(0, 5, title, ln=True)
        self.set_fill_color(30, 30, 45)
        self.set_text_color(200, 230, 255)
        self.set_font('Courier', '', 7.5)
        self.set_x(15)
        block_w = 180
        # measure total height
        total_h = len(lines) * 4.5 + 4
        self.rect(15, self.get_y(), block_w, total_h, 'F')
        self.set_y(self.get_y() + 2)
        for line in lines:
            self.set_x(17)
            self.cell(block_w - 4, 4.5, line, ln=True)
        self.set_text_color(*BLACK)
        self.ln(2)

    def info_box(self, title, text, color=LIGHT_BG, border_color=SECONDARY):
        self.ln(2)
        self.set_fill_color(*color)
        self.set_draw_color(*border_color)
        self.set_line_width(0.5)
        x0 = 15
        w  = 180
        # estimate height
        self.set_font('Helvetica', '', 9)
        lines_n = len(self.multi_cell(w-6, 5, text, split_only=True))
        h = lines_n * 5 + 12
        self.rect(x0, self.get_y(), w, h, 'FD')
        self.set_font('Helvetica', 'B', 9)
        self.set_text_color(*border_color)
        self.set_x(x0 + 3)
        self.cell(0, 6, title, ln=True)
        self.set_font('Helvetica', '', 9)
        self.set_text_color(*BLACK)
        self.set_x(x0 + 3)
        self.multi_cell(w - 6, 5, text)
        self.ln(2)

    # -- table -----------------------------------------------------------------
    def table(self, headers, rows, col_widths=None):
        if col_widths is None:
            col_widths = [180 // len(headers)] * len(headers)
        self.set_font('Helvetica', 'B', 8)
        self.set_fill_color(*PRIMARY)
        self.set_text_color(*WHITE)
        self.set_draw_color(*GRAY_LIGHT)
        self.set_line_width(0.2)
        self.set_x(15)
        for i, h in enumerate(headers):
            self.cell(col_widths[i], 6, h, border=1, fill=True)
        self.ln()
        self.set_font('Helvetica', '', 8)
        self.set_text_color(*BLACK)
        for r_idx, row in enumerate(rows):
            self.set_fill_color(*LIGHT_BG if r_idx % 2 == 0 else WHITE)
            self.set_x(15)
            for i, cell in enumerate(row):
                self.cell(col_widths[i], 5.5, str(cell), border=1, fill=True)
            self.ln()
        self.ln(2)

    # -- TOC -------------------------------------------------------------------
    def toc_entry(self, number, title, page):
        self.set_font('Helvetica', '', 10)
        self.set_text_color(*BLACK)
        self.set_x(20)
        dots = '.' * max(0, 80 - len(f'{number} {title}'))
        self.cell(0, 6, f'{number}  {title} {dots} {page}', ln=True)

    def toc_sub(self, number, title, page):
        self.set_font('Helvetica', '', 9)
        self.set_text_color(*GRAY)
        self.set_x(28)
        dots = '.' * max(0, 77 - len(f'{number} {title}'))
        self.cell(0, 5, f'{number}  {title} {dots} {page}', ln=True)


# ===============================================================================
def build_pdf():
    pdf = DocPDF(orientation='P', unit='mm', format='A4')
    pdf.set_auto_page_break(auto=True, margin=18)
    pdf.set_margins(15, 18, 15)

    # -- COVER -----------------------------------------------------------------
    pdf.cover_page()

    # -- TABLE OF CONTENTS -----------------------------------------------------
    pdf.add_page()
    pdf.set_fill_color(*PRIMARY)
    pdf.set_text_color(*WHITE)
    pdf.set_font('Helvetica', 'B', 16)
    pdf.cell(0, 12, '  Tabla de Contenidos', fill=True, ln=True)
    pdf.set_text_color(*BLACK)
    pdf.ln(4)

    toc = [
        ('1', 'Introducción y Contexto Clínico',            3),
        ('', '1.1 Radioterapia de Intensidad Modulada',     3),
        ('', '1.2 Problemas de Optimización en IMRT',       3),
        ('2', 'Arquitectura General del Sistema',           4),
        ('', '2.1 Estructura de Archivos',                  4),
        ('', '2.2 Pipeline de Ejecución',                   4),
        ('3', 'Framework EMILI',                            5),
        ('', '3.1 Clases Base del Framework',               5),
        ('', '3.2 Algoritmos Metaheurísticos',              6),
        ('', '3.3 Criterios de Terminación',                7),
        ('4', 'Representación de la Instancia IMRT',        8),
        ('', '4.1 Carga de Datos',                          8),
        ('', '4.2 Estructuras de Datos Clave',              8),
        ('', '4.3 Formatos Soportados',                     9),
        ('5', 'Subproblema FMO (Fluence Map Optimization)', 10),
        ('', '5.1 Formulación del Problema',                10),
        ('', '5.2 Solución Inicial',                        10),
        ('', '5.3 Estructuras de Vecindad FMO',             11),
        ('', '5.4 Perturbaciones y Aceptación',             11),
        ('6', 'Subproblema BAO (Beam Angle Optimization)',  12),
        ('', '6.1 Formulación del Problema',                12),
        ('', '6.2 Solución Inicial BAO',                    12),
        ('', '6.3 Vecindad de Intercambio de Ángulos',      13),
        ('', '6.4 Solver OSQP para FMO Exacto',             13),
        ('7', 'Constructor de Componentes (ImrtBuilder)',   15),
        ('', '7.1 Palabras Clave Disponibles',              15),
        ('8', 'Gramáticas de Configuración (irace/GGA)',    16),
        ('', '8.1 Estructura de Gramática',                 16),
        ('', '8.2 Parámetros Numéricos Configurables',      17),
        ('9', 'Flujo de Ejecución Detallado',               18),
        ('', '9.1 Flujo BAO+FMO Completo',                  18),
        ('', '9.2 Reportes Clínicos ICRU-83',               19),
        ('10','Resumen de Diseño y Patrones',               20),
    ]
    for number, title, page in toc:
        if number:
            pdf.toc_entry(number, title, page)
        else:
            pdf.toc_sub('', title, page)

    # ==========================================================================
    # SECTION 1 -- INTRODUCCIÓN
    # ==========================================================================
    pdf.add_page()
    pdf.section('1', 'Introducción y Contexto Clínico')

    pdf.subsection('1.1  Radioterapia de Intensidad Modulada (IMRT)')
    pdf.body(
        'La Radioterapia de Intensidad Modulada (IMRT) es una técnica avanzada de tratamiento oncológico '
        'que permite administrar dosis de radiación con alta precisión espacial. El objetivo es '
        'maximizar la dosis absorbida en el volumen tumoral (PTV, Planning Target Volume) mientras se '
        'minimizan los daños en los órganos de riesgo adyacentes (OARs, Organs At Risk). '
        'Para ello, un acelerador lineal gira alrededor del paciente a distintos ángulos de gantry, '
        'emitiendo haces subdivididos en pequeñas bixeles denominados beamlets. '
        'Las intensidades de estos beamlets son las variables de decisión del problema de optimización.'
    )

    pdf.subsection('1.2  Problemas de Optimización en IMRT')
    pdf.body(
        'El proceso de planificación de tratamiento involucra dos subproblemas estrechamente relacionados:'
    )
    pdf.bullet('FMO - Fluence Map Optimization: dado un conjunto fijo de K ángulos de gantry, optimizar '
               'las intensidades continuas de los beamlets para minimizar las penalizaciones de subdosis '
               'en PTV y sobredosis en OARs.')
    pdf.bullet('BAO - Beam Angle Optimization: seleccionar el conjunto óptimo de K ángulos de gantry '
               'de entre n candidatos, de manera que la FMO resultante sea lo más eficaz posible. '
               'Es un problema combinatorio NP-difícil (C(n,K) subconjuntos posibles).')
    pdf.ln(2)
    pdf.info_box(
        'Jerarquía de Optimización',
        'BAO (combinatorio, exterior) -> selecciona K ángulos\n'
        'FMO (continuo cuadrático, interior) -> optimiza intensidades para esos K ángulos\n'
        'La combinación de ambos subproblemas se resuelve mediante metaheurísticas (BAO) '
        'con solución exacta del FMO vía OSQP en cada evaluación.'
    )

    # ==========================================================================
    # SECTION 2 -- ARQUITECTURA
    # ==========================================================================
    pdf.add_page()
    pdf.section('2', 'Arquitectura General del Sistema')

    pdf.subsection('2.1  Estructura de Archivos del Proyecto')
    pdf.table(
        ['Archivo / Directorio', 'Descripción'],
        [
            ['main.cpp',                    'Punto de entrada; parseo de argumentos y ejecución'],
            ['emilibase.h/cpp',             'Framework EMILI: clases base para metaheurísticas'],
            ['generalParser.h/cpp',         'Parser de configuración y tokens de línea de comandos'],
            ['imrt/imrt.h/cpp',             'Problema FMO, soluciones, vecindades de beamlets'],
            ['imrt/imrt_instance.h/cpp',    'Carga de instancias, cálculo de dosis, reportes ICRU-83'],
            ['imrt/imrt_builder.h/cpp',     'Fábrica de componentes IMRT desde tokens CLI'],
            ['imrt/imrt_bao.h/cpp',         'Problema BAO: selección de ángulos, vecindad de intercambio'],
            ['imrt/imrt_fmo.h/cpp',         'Solver OSQP: resolución exacta del QP de FMO'],
            ['imrt_grammar*.xml',           'Gramáticas XML para configuración automática con irace/GGA'],
            ['CMakeLists.txt',              'Configuración de compilación (WITH_OSQP, flags, targets)'],
        ],
        col_widths=[70, 110]
    )

    pdf.subsection('2.2  Pipeline de Ejecución')
    pdf.body('El flujo de ejecución desde la línea de comandos es el siguiente:')
    pdf.code_block([
        './emili_imrt  [problema]  [solución_inicial]  [metaheurística]  [parámetros]',
        '',
        'Ejemplos:',
        '  # BAO con ILS + intercambio de ángulos:',
        '  ./emili_imrt baoimrt 5 instances/PROSTATE/ ifirstk nangswap ils locmin prangswap aimprove',
        '',
        '  # FMO con búsqueda por primera mejora + ILS:',
        '  ./emili_imrt imrt instances/PROSTATE/ izero nshift 0.5 ils locmin prandom 5 10 aimprove',
    ], 'Invocación desde CLI')

    pdf.body(
        'El componente GeneralParserE tokeniza los argumentos y los redirige a EmBaseBuilder '
        '(componentes del framework) o ImrtBuilder (componentes específicos de IMRT). '
        'Una vez construidos todos los componentes, se ejecuta el algoritmo con '
        'ls->timedSearch(seconds) o ls->search(), y al final se genera el reporte clínico.'
    )

    # ==========================================================================
    # SECTION 3 -- FRAMEWORK EMILI
    # ==========================================================================
    pdf.add_page()
    pdf.section('3', 'Framework EMILI (emilibase.h/cpp)')

    pdf.subsection('3.1  Clases Base Abstractas')
    pdf.body(
        'EMILI define una jerarquía de clases abstractas que representan los bloques funcionales '
        'de cualquier metaheurística. Cada componente es una estrategia intercambiable:'
    )

    pdf.table(
        ['Clase Abstracta', 'Responsabilidad', 'Método Principal'],
        [
            ['Problem',          'Evaluador de función objetivo',      'calcObjectiveFunctionValue(Solution&)'],
            ['Solution',         'Contenedor de una solución',         'clone(), isFeasible(), getSolutionRepresentation()'],
            ['InitialSolution',  'Generador de solución inicial',      'generateSolution(), generateEmptySolution()'],
            ['Neighborhood',     'Generador de movimientos/vecinos',   'computeStep(Solution*), reverseLastMove(Solution*)'],
            ['Perturbation',     'Mecanismo de escape de mínimo local','perturb(Solution*)'],
            ['Acceptance',       'Criterio de aceptación de solución', 'accept(current, candidate)'],
            ['Termination',      'Condición de parada',                'isSatisfied()'],
            ['LocalSearch',      'Motor de búsqueda local',            'search(), timedSearch(seconds)'],
        ],
        col_widths=[38, 68, 74]
    )

    pdf.subsection('3.2  Algoritmos Metaheurísticos Implementados')

    pdf.subsubsection('Búsqueda Local Simple')
    pdf.bullet('FirstImprovementSearch: acepta el primer vecino que mejore la solución actual.')
    pdf.bullet('BestImprovementSearch: explora toda la vecindad y acepta el mejor vecino.')
    pdf.bullet('TieBrakingBestImprovementSearch: desempate con un objetivo secundario.')

    pdf.subsubsection('Iterated Local Search (ILS)')
    pdf.body(
        'Estructura: LocalSearch + Perturbation + Acceptance. Flujo:'
    )
    pdf.code_block([
        's = initial_solution()',
        's = local_search(s)',
        'loop until termination:',
        '    s\' = perturb(s)',
        '    s\' = local_search(s\')',
        '    s  = accept(s, s\')',
        'return best_so_far',
    ], 'Pseudocódigo ILS')

    pdf.subsubsection('Tabu Search')
    pdf.bullet('BestTabuSearch / FirstTabuSearch: búsqueda local con memoria tabú.')
    pdf.bullet('ImrtTabuMemory: buffer circular de vectores de intensidad con tenure configurable.')
    pdf.bullet('GeneralTabuSearch: envuelve cualquier LocalSearch con restricciones tabú.')

    pdf.subsubsection('Variable Neighborhood Descent / Search (VND/VNS)')
    pdf.bullet('VNDSearch: alterna entre vecindades; si mejora en vecindad k -> resetear a k=0, si no -> k++.')
    pdf.bullet('GVNS (General VNS): combina shake (perturbación) + NeighborhoodChange (aceptación con incremento de k).')
    pdf.bullet('PerShake: usa objetos Perturbation como shakes para VNS.')

    pdf.subsubsection('Simulated Annealing')
    pdf.bullet('MetropolisAcceptance: temperatura fija, criterio de aceptación estocástico.')
    pdf.bullet('Metropolis: esquema de enfriamiento progresivo con parámetros start_temp, end_temp, temp_ratio.')

    pdf.subsection('3.3  Criterios de Terminación')
    pdf.table(
        ['Clase', 'Condición de Parada'],
        [
            ['LocalMinimaTermination',  'Detiene al no encontrar mejora (mínimo local)'],
            ['WhileTrueTermination',    'Nunca termina (uso con timer externo)'],
            ['TimedTermination',        'Detiene al superar tiempo de pared dado'],
            ['MaxStepsTermination',     'Detiene tras N iteraciones'],
            ['MaxEvaluations',          'Detiene tras N evaluaciones de función objetivo'],
            ['MaxStepsOrLocmin',        'Combinado: N pasos O mínimo local'],
            ['ImrtMaxIterations',       'Cuenta intercambios de ángulos (específico para BAO)'],
            ['ImrtFeasibleTermination', 'Detiene si objetivo <= tolerancia (convergencia a factibilidad)'],
        ],
        col_widths=[70, 110]
    )

    # ==========================================================================
    # SECTION 4 -- INSTANCIA
    # ==========================================================================
    pdf.add_page()
    pdf.section('4', 'Representación de la Instancia IMRT (imrt_instance.h/cpp)')

    pdf.subsection('4.1  Carga de Datos')
    pdf.body(
        'La clase ImrtInstance gestiona la carga y organización de los datos del problema. '
        'Soporta dos formatos de archivo distintos:'
    )
    pdf.bullet('Formato clásico: un archivo de texto por cada par (órgano, ángulo) con nombre '
               'ORGANNAME_<angulo>.txt, más un instance_config.txt que lista ángulos, '
               'órganos, pesos y restricciones.')
    pdf.bullet('Formato CORT: archivos VOIList por órgano y matrices Dij completas por ángulo '
               '(Gantry<g>_Couch<c>_D.txt), con detección automática del formato.')
    pdf.ln(2)
    pdf.body('El archivo de configuración instance_config.txt acepta las siguientes directivas:')
    pdf.code_block([
        'angles   0 40 80 120 160 200 240 280 320     # ángulos de gantry (grados)',
        'dimlets  625                                  # beamlets por ángulo',
        'ptv      PROSTATE  Dmin=74.0                 # PTV con dosis mínima',
        'oar      BLADDER   Dmax=72.0                 # OAR con dosis máxima',
        'oar      RECTUM    Dmax=70.0 vol_frac=0.25 dose_limit=65.0',
        'w_under  1.0                                 # peso subdosis PTV',
        'w_over   0.1                                 # peso sobredosis OAR',
        'max_intensity 10.0                           # cota superior beamlets',
    ], 'instance_config.txt')

    pdf.subsection('4.2  Estructuras de Datos Clave')
    pdf.table(
        ['Estructura', 'Campos Principales', 'Descripción'],
        [
            ['DoseEntry',   'boxet_id, dimlet_id, dose_rate',
             'Entrada de matriz dispersa: beamlet -> dosis en vóxel'],
            ['OrganData',   'name, is_ptv, Dmin, Dmax, entries[]',
             'Órgano con restricciones de dosis y matriz dispersa'],
            ['GridInfo',    'nx, ny, nz, dx, dy, dz, isocenter',
             'Geometría de la rejilla de vóxeles (solo formato CORT)'],
            ['ImrtInstance','n_angles, n_dimlets, organs[], angles[]',
             'Clase contenedora principal del problema'],
        ],
        col_widths=[32, 60, 88]
    )

    pdf.subsection('4.3  Formatos de Instancia Soportados')
    pdf.body(
        'Ambos formatos utilizan matrices de dosis dispersas (DoseEntry) para evitar el '
        'almacenamiento de la matriz Dij completa (que puede tener millones de entradas). '
        'La función computeOrganDoses(x) calcula el vector de dosis d = D·x recorriendo '
        'solo las entradas no nulas, con complejidad O(entradas_no_nulas).'
    )

    # ==========================================================================
    # SECTION 5 -- FMO
    # ==========================================================================
    pdf.add_page()
    pdf.section('5', 'Subproblema FMO - Fluence Map Optimization (imrt.h/cpp)')

    pdf.subsection('5.1  Formulación del Problema')
    pdf.body(
        'La clase ImrtProblem implementa la función objetivo cuadrática penalizada '
        'para la optimización de mapas de fluencia:'
    )
    pdf.code_block([
        'f(x) = w_under * sum_{b in PTV}  max(0, Dmin - d_b)²',
        '      + w_over  * sum_{o in OARs} sum_{b in o} max(0, d_b - Dmax)²',
        '',
        'donde  d_b = sum_j  D[b,j] · x[j]   (dosis en vóxel b)',
        '       x[j] >= 0                    (intensidades no negativas)',
        '       x[j] <= max_intensity        (cota superior)',
    ], 'Función Objetivo FMO (penalización cuadrática)')

    pdf.body(
        'ImrtSolution almacena el vector de intensidades x in R^n (n = n_angles × dimlets_per_angle). '
        'El campo active_angle_idxs_ de ImrtProblem permite restringir la optimización a un '
        'subconjunto de ángulos, lo cual es usado por el flujo BAO.'
    )

    pdf.subsection('5.2  Soluciones Iniciales para FMO')
    pdf.table(
        ['Clase', 'Token CLI', 'Descripción'],
        [
            ['ZeroInitialSolution',    'izero',           'Todas las intensidades x[j] = 0'],
            ['UniformInitialSolution', 'iuniform <v>',    'Todas las intensidades x[j] = v (respeta ángulos activos)'],
            ['RandomInitialSolution',  'irandom <max>',   'Intensidades x[j] ~ U[0, max] (aleatorio uniforme)'],
        ],
        col_widths=[58, 38, 84]
    )

    pdf.subsection('5.3  Estructuras de Vecindad para FMO')

    pdf.subsubsection('SingleBeamletShift (token: nshift <delta>)')
    pdf.body(
        'Mueve un beamlet a la vez modificando su intensidad en ±delta, con clamping a [0, max_intensity]. '
        'Tamaño de vecindad: 2 × n_dimlets (o 2 × |ángulos_activos × dimlets_por_ángulo| en modo BAO). '
        'Implementa exploración sistemática recorriendo todos los dimlets en ambas direcciones.'
    )

    pdf.subsubsection('BeamletSwap (token: nswap)')
    pdf.body(
        'Intercambia las intensidades de dos beamlets distintos. '
        'Tamaño de vecindad: n_dimlets × (n_dimlets - 1) / 2. '
        'Útil para reorganizar la distribución de fluencia sin cambiar su suma total.'
    )

    pdf.body('Ambas vecindades exponen la interfaz de iterador:')
    pdf.code_block([
        'for (auto it = nbh.begin(sol); it != nbh.end(); ++it) {',
        '    Solution* neighbor = *it;',
        '    if (neighbor->solution_value < best_val) { ... }',
        '    nbh.reverseLastMove(sol);  // deshacer si no se acepta',
        '}',
    ], 'Iteración sobre la vecindad')

    pdf.subsection('5.4  Perturbaciones y Criterio de Aceptación')
    pdf.table(
        ['Componente', 'Clase', 'Descripción'],
        [
            ['Perturbación',  'RandomBeamletPerturbation',  'k beamlets aleatorios -> U[0, max_intensity]'],
            ['Aceptación',    'ImrtImproveAccept',          'Acepta solo si la nueva solución mejora la actual'],
            ['Aceptación SA', 'MetropolisAcceptance',       'Acepta con prob. exp(-deltaf/T) (Simulated Annealing)'],
        ],
        col_widths=[35, 60, 85]
    )

    # ==========================================================================
    # SECTION 6 -- BAO
    # ==========================================================================
    pdf.add_page()
    pdf.section('6', 'Subproblema BAO - Beam Angle Optimization (imrt_bao.h/cpp)')

    pdf.subsection('6.1  Formulación del Problema BAO')
    pdf.body(
        'El problema BAO busca el conjunto A* subset {0,...,n-1} de K ángulos que minimiza el '
        'valor óptimo del FMO correspondiente. Se formula como:'
    )
    pdf.code_block([
        'min_{A subset {0,...,n-1}, |A|=K}   f*(A)  =  min_{x>=0} FMO(x, A)',
        '',
        'Espacio de búsqueda: C(n, K) subconjuntos posibles.',
        'Ejemplo: n=36 ángulos candidatos, K=5 -> C(36,5) = 376,992 subconjuntos.',
        'Evaluación de cada subconjunto: resolver QP exacto mediante OSQP.',
    ], 'Formulación BAO')

    pdf.body(
        'La clase BaoProblem encapsula este evaluador. Cada llamada a '
        'calcObjectiveFunctionValue(s) invoca ImrtFmoSolver::solve(active_angles), '
        'que resuelve el QP con OSQP y retorna el valor óptimo f* junto con '
        'el vector de intensidades óptimo x*.'
    )

    pdf.subsection('6.2  Soluciones Iniciales BAO')
    pdf.table(
        ['Clase', 'Token CLI', 'Descripción'],
        [
            ['FirstKAnglesInit',  'ifirstk',   'Selecciona los primeros K ángulos: {0, 1, ..., K-1}'],
            ['RandomKAnglesInit', 'irandomk',  'Fisher-Yates sobre todos los ángulos, toma K primeros y ordena'],
        ],
        col_widths=[50, 30, 100]
    )
    pdf.body(
        'BaoSolution almacena: (1) active_angles_: vector de K índices de ángulo (base 0); '
        '(2) angle_degrees_: valores en grados para visualización; '
        '(3) intensities_: vector de n_dimlets con las intensidades óptimas (las inactivas = 0).'
    )

    pdf.subsection('6.3  Vecindad de Intercambio de Ángulos (AngleSwapNeighborhood)')
    pdf.body(
        'Cada movimiento consiste en reemplazar un ángulo activo por un ángulo inactivo. '
        'El tamaño de la vecindad es K × (n - K). Al generar un vecino, se invoca automáticamente '
        'el solver OSQP para obtener f*(vecino). La interface implementada es:'
    )
    pdf.code_block([
        'computeStep(base_sol):',
        '  activo[i] <- inactivo[j]         // intercambio de ángulo',
        '  -> llamar OSQP::solve(new_angles) // resolver FMO exacto',
        '  -> retornar BaoSolution con nuevo conjunto y f*',
        '',
        'reverseLastMove(sol):',
        '  restaurar active_angles_ del estado previo (intensities quedan desactualizadas)',
        '',
        'random(sol):',
        '  seleccionar activo[i] e inactivo[j] al azar -> evaluar con OSQP',
    ], 'AngleSwapNeighborhood')

    pdf.info_box(
        'Nota sobre complejidad',
        'Cada evaluación de vecino requiere resolver un QP cuadrático con OSQP. '
        'La variable WITH_OSQP debe estar activada en CMake para compilar el módulo BAO. '
        'El tiempo de solución de OSQP escala con el número de beamlets activos (K × dimlets_per_angle) '
        'y el número de vóxeles en todos los órganos.'
    )

    pdf.subsection('6.4  Perturbaciones para BAO')
    pdf.table(
        ['Clase', 'Token CLI', 'Descripción'],
        [
            ['RandomAnglesPerturbation', 'prangswap',
             'Intercambia p ángulos activos aleatoriamente con el pool inactivo (Fisher-Yates parcial)'],
        ],
        col_widths=[55, 35, 90]
    )

    # -- OSQP -----------------------------------------------------------------
    pdf.add_page()
    pdf.section('6', 'Solver OSQP para FMO Exacto (imrt_fmo.h/cpp)', color=SECONDARY)

    pdf.subsection('Formulación QP con Variables de Holgura')
    pdf.body(
        'ImrtFmoSolver reformula el problema FMO como un Programa Cuadrático (QP) estándar '
        'añadiendo variables de holgura para linealizar las funciones max():'
    )
    pdf.code_block([
        'Variables:  z = [x_activo in R^(K·m);  u_ptv in R^|PTV|;  v_oar in R^|OAR|]',
        '',
        'min   w_under * ||u||²  +  w_over * ||v||²',
        '',
        's.t.  D_ptv * x_activo + u  >=  Dmin    (G4: subdosis PTV)',
        '      D_oar * x_activo - v  <=  Dmax    (G5: sobredosis OAR)',
        '      0  <=  x_j  <=  max_intensity      (G1: cotas beamlets)',
        '      u, v  >=  0                        (G2, G3: holguras no negativas)',
    ], 'Formulación QP para OSQP')

    pdf.subsection('Construcción del QP y Configuración de OSQP')
    pdf.body(
        'En cada llamada a solve(active_angles), se construye el QP de tamaño reducido '
        '(solo K ángulos activos) en formato CSC (Compressed Sparse Column):'
    )
    pdf.bullet('Matriz P (Hessiana): diagonal, con 0 en bloque x, w_under en bloque u, w_over en bloque v.')
    pdf.bullet('Matriz A (restricciones): combina G1 (identidad sparse), G4 (filas PTV) y G5 (filas OAR).')
    pdf.bullet('Vectores q, l, u: vector lineal cero (P ya es cuadrático), cotas inferiores/superiores.')
    pdf.ln(2)
    pdf.body('Parámetros de OSQP configurados para alta precisión:')
    pdf.code_block([
        'settings.verbose       = 0        // sin salida por consola',
        'settings.warm_starting = 0        // reinicio limpio en cada llamada',
        'settings.eps_abs       = 1e-6     // tolerancia absoluta',
        'settings.eps_rel       = 1e-6     // tolerancia relativa',
        'settings.max_iter      = 10000    // iteraciones máximas',
        'settings.polishing     = 1        // pulido de solución para máxima precisión',
    ], 'Configuración de OSQP')

    pdf.subsection('Extracción e Integración de la Solución')
    pdf.body(
        'Tras la resolución, se extrae x* del vector solución de OSQP y se mapea de vuelta '
        'al vector completo de n_dimlets (los beamlets de ángulos inactivos quedan en 0). '
        'La función retorna {x_full, f*}, que son almacenados en BaoSolution.'
    )

    # ==========================================================================
    # SECTION 7 -- BUILDER
    # ==========================================================================
    pdf.add_page()
    pdf.section('7', 'Constructor de Componentes - ImrtBuilder (imrt_builder.h/cpp)')

    pdf.subsection('7.1  Palabras Clave Disponibles')
    pdf.body(
        'ImrtBuilder implementa el patrón Builder para construir todos los componentes '
        'IMRT a partir de tokens de línea de comandos. Hereda de la clase Builder del framework.'
    )

    pdf.subsubsection('Palabras Clave de Problema')
    pdf.table(
        ['Token', 'Clase Creada', 'Parámetros'],
        [
            ['imrt <dir> [nactive K] [verbose]',
             'ImrtProblem',
             'dir: directorio de instancia; nactive K: restringir a K primeros ángulos'],
            ['baoimrt <K> <dir> [verbose]',
             'BaoProblem',
             'K: número de ángulos a seleccionar; dir: directorio de instancia; requiere WITH_OSQP=ON'],
        ],
        col_widths=[65, 35, 80]
    )

    pdf.subsubsection('Soluciones Iniciales')
    pdf.table(
        ['Token', 'Modo', 'Clase'],
        [
            ['izero',           'FMO',  'ZeroInitialSolution'],
            ['iuniform <v>',    'FMO',  'UniformInitialSolution(v)'],
            ['irandom <max>',   'FMO',  'RandomInitialSolution(max)'],
            ['ifirstk',         'BAO',  'FirstKAnglesInit'],
            ['irandomk',        'BAO',  'RandomKAnglesInit'],
        ],
        col_widths=[40, 25, 115]
    )

    pdf.subsubsection('Vecindades, Perturbaciones y Terminación')
    pdf.table(
        ['Token', 'Tipo', 'Descripción'],
        [
            ['nshift <delta>',      'Vecindad FMO',    'SingleBeamletShift con paso delta'],
            ['nswap',               'Vecindad FMO',    'BeamletSwap: intercambio de dos beamlets'],
            ['nangswap',            'Vecindad BAO',    'AngleSwapNeighborhood: intercambio de ángulos'],
            ['prandom <k> <max>',   'Perturbación FMO','k beamlets aleatorios a U[0, max]'],
            ['prangswap',           'Perturbación BAO','RandomAnglesPerturbation: swap de ángulos'],
            ['aimprove',            'Aceptación',      'ImrtImproveAccept: solo mejoras'],
            ['tmaxiter <n>',        'Terminación BAO', 'Detener tras n intercambios de ángulos'],
            ['tfeasible',           'Terminación FMO', 'Detener si objetivo <= tolerancia'],
        ],
        col_widths=[40, 35, 105]
    )

    # ==========================================================================
    # SECTION 8 -- GRAMÁTICAS
    # ==========================================================================
    pdf.add_page()
    pdf.section('8', 'Gramáticas de Configuración XML (imrt_grammar*.xml)')

    pdf.subsection('8.1  Propósito y Estructura')
    pdf.body(
        'Las gramáticas XML definen el espacio de configuración de algoritmos para herramientas '
        'de autoajuste como irace (R) o GGA (Genetic-Guided Algorithm). Cada gramática describe '
        'producciones BNF donde los valores numéricos tienen rangos y pasos configurables.'
    )
    pdf.body('El sistema incluye cuatro archivos de gramática principales:')
    pdf.table(
        ['Archivo', 'Variante'],
        [
            ['imrt_grammar.xml',         'Gramática principal: todos los metaheurísticos'],
            ['imrt_grammar_repair.xml',  'Terminación fija (maxstep 999-1000)'],
            ['imrt_grammar_time.xml',    'Terminación basada en tiempo de pared'],
            ['grámaticas/imrt_grammar_all.xml', 'Variante completa con todos los tipos'],
        ],
        col_widths=[70, 110]
    )

    pdf.subsection('8.2  Parámetros Numéricos Configurables')
    pdf.table(
        ['Parámetro', 'Rango', 'Paso', 'Uso'],
        [
            ['delta',           '[0.1, 5.0]',       '0.1',    'Tamaño de paso en SingleBeamletShift'],
            ['intensity',       '[0.5, 10.0]',      '0.5',    'Intensidad inicial uniforme'],
            ['max_intensity',   '[1.0, 10.0]',      '1.0',    'Cota superior de beamlets'],
            ['metrotemp',       '[0.1, 300]',       '1',      'Temperatura Metropolis (fija)'],
            ['tabutenure_size', '[1, 200]',         '1',      'Tamaño de la memoria tabú'],
            ['random_moves',    '[1, 100]',         '1',      'Movimientos en perturbación aleatoria'],
            ['start_temp',      '[1.1, 50000]',     '0.0001', 'Temperatura inicial SA'],
            ['end_temp',        '[0, 1.0]',         '0.0001', 'Temperatura final SA'],
            ['temp_ratio',      '[0.0001, 1]',      '0.0001', 'Ratio de enfriamiento SA'],
            ['tempiterations',  '[1, 5000]',        '1',      'Iteraciones por temperatura SA'],
            ['steps',           '[1000, 100000]',   '1000',   'Número máximo de pasos'],
        ],
        col_widths=[38, 28, 22, 92]
    )

    pdf.subsection('8.3  Metaheurísticos Derivados')
    pdf.table(
        ['Token', 'Clase EMILI', 'Descripción'],
        [
            ['first',  'FirstImprovementSearch',  'Primera mejora'],
            ['best',   'BestImprovementSearch',   'Mejor mejora'],
            ['ils',    'IteratedLocalSearch',      'first/best + terminación + perturbación + aceptación'],
            ['tabu',   'BestTabuSearch',           'Búsqueda tabú con memoria configurable'],
            ['vnd',    'VNDSearch',                'VND con 3 vecindades (nshift 1, nshift 0.5, nswap)'],
            ['vns',    'GVNS',                     'VNS general con PerShake y NeighborhoodChange'],
        ],
        col_widths=[22, 55, 103]
    )

    # ==========================================================================
    # SECTION 9 -- FLUJO DETALLADO
    # ==========================================================================
    pdf.add_page()
    pdf.section('9', 'Flujo de Ejecución Detallado')

    pdf.subsection('9.1  Flujo BAO + FMO Completo')
    pdf.code_block([
        'INICIALIZACIÓN',
        '  1. ImrtInstance::loadFromDirectory(dir)',
        '     |-- detectar formato (clásico / CORT)',
        '     |-- cargar config: ángulos, dimlets, órganos, pesos',
        '     |-- construir matrices de dosis dispersas (DoseEntry[])',
        '',
        '  2. ImrtFmoSolver(inst)',
        '     |-- pre-computar índices PTV/OAR',
        '     |-- pre-computar cotas fijas del QP',
        '',
        '  3. BaoProblem(inst, fmo, K)',
        '',
        'SOLUCIÓN INICIAL',
        '  4. FirstKAnglesInit / RandomKAnglesInit -> BaoSolution(active=[0..K-1])',
        '     |-- BaoProblem::evaluateSolution(s) -> OSQP::solve(angles) -> f*, x*',
        '',
        'BÚSQUEDA (ejemplo ILS)',
        '  5. LocalSearch::search(initial_sol)',
        '     loop until termination:',
        '       a. AngleSwapNeighborhood::computeStep(s):',
        '          |-- reemplazar activo[i] por inactivo[j]',
        '          |-- OSQP::solve(new_angles) -> evaluar f*, x*',
        '       b. Si mejora -> actualizar solución actual',
        '       c. Si mínimo local:',
        '          |-- RandomAnglesPerturbation::perturb(s) -> nueva configuración',
        '          |-- LocalSearch::search(perturbed) -> nueva búsqueda local',
        '       d. ImrtImproveAccept::accept(current, candidate)',
        '',
        'RESULTADO',
        '  6. Reportar mejor BaoSolution:',
        '     |-- ángulos seleccionados (grados)',
        '     |-- valor objetivo final f*',
        '     |-- ImrtInstance::reportPlan(s) -> estadísticas clínicas ICRU-83',
    ], 'Pseudocódigo BAO + FMO con ILS')

    pdf.subsection('9.2  Reportes Clínicos ICRU-83 (reportPlan)')
    pdf.body(
        'Tras la optimización, main.cpp invoca reportPlan() sobre la instancia para generar '
        'un informe clínico completo conforme al estándar ICRU-83:'
    )
    pdf.table(
        ['Métrica', 'Fórmula / Descripción'],
        [
            ['Dmin, Dmean, Dmax',   'Dosis mínima, media y máxima por órgano (Gy)'],
            ['D95, D5, D2',         'Dosis cubriendo el 95%, 5% y 2% del volumen del órgano'],
            ['V95 (cobertura)',      'Fracción del PTV con dosis >= 95% de la prescripción'],
            ['Conformity Index (CI)','V_Rx / V_PTV (volumen con dosis >= prescripción / volumen PTV)'],
            ['Homogeneity Index (HI)','(D2 - D98) / D_Rx (dispersión de dosis en PTV)'],
            ['DVH CSV',              'Curvas DVH por órgano exportadas a dvh.csv'],
            ['Tabla de restricciones','DVH goals vs. valores obtenidos, marcando OK o VIOLACIÓN'],
        ],
        col_widths=[52, 128]
    )

    # ==========================================================================
    # SECTION 10 -- PATRONES DE DISEÑO
    # ==========================================================================
    pdf.add_page()
    pdf.section('10', 'Resumen de Diseño y Patrones Arquitectónicos')

    pdf.subsection('10.1  Patrones de Diseño Utilizados')
    pdf.table(
        ['Patrón', 'Uso en EMILI-IMRT'],
        [
            ['Template Method',     'LocalSearch define el esquema; subclases sobrescriben variantes de búsqueda'],
            ['Strategy',            'Neighborhood, Perturbation y Acceptance son estrategias intercambiables'],
            ['Builder',             'ImrtBuilder construye componentes IMRT desde tokens CLI'],
            ['Iterator',            'NeighborhoodIterator permite recorrer vecindades con for-range'],
            ['Polymorphism',        'Problem, Solution, Termination permiten extensión específica de dominio'],
            ['Sparse Matrix',       'DoseEntry evita almacenar la matriz Dij completa (ahorro de memoria)'],
            ['Grammar-based Config','Gramáticas XML + irace/GGA para síntesis automática de algoritmos'],
        ],
        col_widths=[42, 138]
    )

    pdf.subsection('10.2  Relación entre Componentes Principales')
    pdf.code_block([
        'main.cpp',
        '  |-- GeneralParserE',
        '        |-- EmBaseBuilder -> construye LocalSearch, ILS, Tabu, VNS, VND',
        '        |-- ImrtBuilder   -> construye Problem, InitSol, Neighborhood, Perturbation',
        '',
        'BaoProblem (ImrtFmoSolver)',
        '  |-- ImrtInstance  (datos clínicos, matrices Dij dispersas)',
        '  |-- OSQP          (resolución QP para cada subconjunto de ángulos)',
        '',
        'Metaheurística (ILS ejemplo)',
        '  |-- LocalSearch      (First/BestImprovement sobre AngleSwapNeighborhood)',
        '  |-- Perturbation     (RandomAnglesPerturbation)',
        '  |-- Acceptance       (ImrtImproveAccept)',
    ], 'Árbol de dependencias simplificado')

    pdf.subsection('10.3  Comparativa de Modos de Operación')
    pdf.table(
        ['Aspecto', 'Modo FMO Puro', 'Modo BAO + FMO'],
        [
            ['Variable de decisión', 'Intensidades x[j] in R^n', 'Subconjunto de K ángulos'],
            ['Espacio de búsqueda',  'Continuo (R^n)',            'Combinatorio (C(n,K))'],
            ['Función objetivo',     'Penalización cuadrática',   'QP exacto vía OSQP'],
            ['Vecindad',             'Shift/Swap de beamlets',    'Intercambio de ángulos'],
            ['Coste por evaluación', 'O(entradas Dij)',           'O(QP solve) >> FMO aprox.'],
            ['Solución inicial',     'Zero / Uniform / Random',   'FirstK / RandomK ángulos'],
            ['Perturbación',         'k beamlets aleatorios',     'p ángulos aleatorios'],
            ['Compilación',          'Siempre disponible',        'Requiere WITH_OSQP=ON'],
        ],
        col_widths=[40, 65, 75]
    )

    pdf.ln(4)
    pdf.info_box(
        'Extensibilidad del Sistema',
        'Para agregar un nuevo metaheurístico: heredar de LocalSearch en emilibase.h y '
        'registrar el token en EmBaseBuilder.\n'
        'Para agregar una nueva vecindad IMRT: heredar de ImrtNeighborhood (imrt.h) e '
        'implementar computeStep() / reverseLastMove(), luego registrar en ImrtBuilder.\n'
        'Para soportar un nuevo formato de instancia: extender ImrtInstance::loadFromDirectory() '
        'con la lógica de detección y carga correspondiente.',
        color=ALT_BG, border_color=ACCENT
    )

    # -- save ------------------------------------------------------------------
    out = '/Users/marrojasr/Documents/code/emili_imrt/EMILI_IMRT_Documentacion.pdf'
    pdf.output(out)
    print(f'PDF generado: {out}')


if __name__ == '__main__':
    build_pdf()
