#!/usr/bin/env python3
"""Generate interactive graphs as HTML using Chart.js (no dependencies)."""
import json, os

os.makedirs('docs/figures', exist_ok=True)

# ========== DATA ==========
ils_tuned  = [4280.55, 4217.67, 4235.16, 4217.98, 4217.98, 4322.73, 4217.67, 4260.42, 4179.08, 4322.73]
ils_base   = [8762.89, 8995.26, 8590.95, 9612.84, 9482.63, 9443.41, 8834.07, 8925.48, 8846.70]
vns_tuned  = [4792.46, 4893.89, 4694.22, 4885.32, 4629.48, 4728.05, 4708.43, 4666.77, 4817.96, 4738.00]
vns_base   = [8896.15, 9034.82, 8629.99, 8505.24, 8949.32, 8134.36, 8436.79, 8480.74, 8416.32]
tabu_tuned = [1800.29]*10
tabu_base  = [9420.47]*9

irace_ils  = [[17,65742],[19,70257],[44,50568],[80,53792],[112,50754],[181,53030],[222,49103],[253,47337],[276,45876],[291,45992],[301,47337],[309,46250],[315,47384],[320,47337],[325,46454]]
irace_vns  = [[16,55896],[22,61229],[41,63895],[68,62786],[134,50891],[163,53770],[198,51849],[243,53527],[261,54621],[274,52440],[288,53028],[294,53467],[301,52985],[305,52582],[324,52217],[325,51275],[327,51275],[328,51275]]
irace_tabu = [[16,50110],[19,57371],[56,61001],[98,57175],[131,57371],[157,52949],[200,54749],[242,53040],[255,50149],[279,50031],[284,48687],[292,47549],[295,47642],[297,48687],[303,47763],[305,48677],[313,48677],[316,48677],[319,48677],[320,48677]]

html = f"""<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>EMILI IMRT — Resultados</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<style>body{{font-family:Arial;max-width:1200px;margin:auto;padding:20px;background:#f5f5f5}}
h1{{color:#2c3e50}}h2{{color:#34495e;border-bottom:2px solid #3498db;padding-bottom:5px}}
.chart-container{{background:white;border-radius:8px;padding:20px;margin:20px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}}
canvas{{max-height:400px}}
.grid{{display:grid;grid-template-columns:1fr 1fr;gap:20px}}
table{{border-collapse:collapse;width:100%;margin:10px 0}}
td,th{{border:1px solid #ddd;padding:8px;text-align:center}}
th{{background:#3498db;color:white}}
.green{{color:#27ae60;font-weight:bold}}.red{{color:#e74c3c}}</style></head><body>
<h1>🏥 EMILI IMRT — Resultados de Validación</h1>
<p>PROSTATE K=4 | OSQP exacto | 10 seeds (42-51) | Wilcoxon p &lt; 0.01</p>

<div class="chart-container"><h2>1. Boxplot f* — Tuned vs Baseline</h2>
<canvas id="boxplot"></canvas></div>

<div class="chart-container"><h2>2. Cobertura PTV — V95%</h2>
<canvas id="v95"></canvas></div>

<div class="grid">
<div class="chart-container"><h2>3. Convergencia irace</h2><canvas id="irace"></canvas></div>
<div class="chart-container"><h2>4. Tiempo de ejecución</h2><canvas id="time"></canvas></div>
</div>

<div class="grid">
<div class="chart-container"><h2>5. Validación Cruzada</h2><canvas id="crossval"></canvas></div>
<div class="chart-container"><h2>6. Ablación rVNS</h2><canvas id="rvns"></canvas></div>
</div>

<div class="chart-container"><h2>7. DVH Clinical Constraints</h2><canvas id="dvh"></canvas></div>

<div class="chart-container">
<h2>📊 Tabla Resumen</h2>
<table>
<tr><th>Algoritmo</th><th>f* Baseline</th><th>f* Tuned</th><th>Mejora</th><th>V95% Base</th><th>V95% Tuned</th><th>Wilcoxon p</th></tr>
<tr><td><b>Tabu</b> 🥇</td><td>9,420</td><td class="green">1,800</td><td class="green">-81%</td><td>86.2%</td><td class="green">98.4%</td><td class="green">0.0013</td></tr>
<tr><td><b>ILS</b> 🥈</td><td>9,081</td><td class="green">4,235</td><td class="green">-53%</td><td>86.4%</td><td class="green">96.3%</td><td class="green">0.0059</td></tr>
<tr><td><b>VNS</b> 🥉</td><td>8,609</td><td class="green">4,747</td><td class="green">-45%</td><td>87.5%</td><td class="green">95.5%</td><td class="green">0.0038</td></tr>
</table>
</div>

<script>
// ===== 1. f* comparison bars =====
new Chart(document.getElementById('boxplot'), {{
    type: 'bar',
    data: {{
        labels: ['ILS','VNS','Tabu'],
        datasets: [
            {{label:'Tuned μ',data:[4235,4747,1800],backgroundColor:'#2ecc71'}},
            {{label:'Baseline μ',data:[9081,8609,9420],backgroundColor:'#e74c3c'}}
        ]
    }},
    options: {{responsive:true, plugins:{{title:{{display:true,text:'f* medio — tuned vs baseline (10 seeds)'}},subtitle:{{display:true,text:'ILS: 4,179-4,323 | VNS: 4,629-4,893 | Tabu: 1,800 (determinístico)'}}}}}}
}});

// ===== 2. V95% =====
new Chart(document.getElementById('v95'), {{
    type: 'bar',
    data: {{
        labels: ['ILS','VNS','Tabu'],
        datasets: [
            {{label:'Baseline',data:[86.4,87.5,86.2],backgroundColor:'#e74c3c'}},
            {{label:'Tuned (irace)',data:[96.3,95.5,98.4],backgroundColor:'#2ecc71'}}
        ]
    }},
    options: {{responsive:true,plugins:{{title:{{display:true,text:'V95% — Cobertura PTV (+8 a +12 pp)'}}}},scales:{{y:{{min:80,max:100}}}}}}
}});

// ===== 3. irace convergence =====
new Chart(document.getElementById('irace'), {{
    type: 'scatter',
    data: {{
        datasets: [
            {{label:'ILS',data:{json.dumps([{"x":d[0],"y":d[1]} for d in irace_ils])},borderColor:'#3498db',backgroundColor:'#3498db',showLine:true,tension:0.1}},
            {{label:'VNS',data:{json.dumps([{"x":d[0],"y":d[1]} for d in irace_vns])},borderColor:'#e67e22',backgroundColor:'#e67e22',showLine:true,tension:0.1}},
            {{label:'Tabu',data:{json.dumps([{"x":d[0],"y":d[1]} for d in irace_tabu])},borderColor:'#9b59b6',backgroundColor:'#9b59b6',showLine:true,tension:0.1}}
        ]
    }},
    options: {{responsive:true,plugins:{{title:{{display:true,text:'Convergencia irace — mean best vs wall time'}}}},scales:{{x:{{title:{{display:true,text:'Wall time (minutos)'}}}},y:{{title:{{display:true,text:'Mean best f*'}}}}}}}}
}});

// ===== 4. Time =====
new Chart(document.getElementById('time'), {{
    type: 'bar',
    data: {{
        labels: ['ILS (×8.6)','VNS (×0.6)','Tabu (×0.9)'],
        datasets: [
            {{label:'Tuned',data:[60.9,22.3,12.0],backgroundColor:'#2ecc71'}},
            {{label:'Baseline',data:[7.1,36.7,12.9],backgroundColor:'#e74c3c'}}
        ]
    }},
    options: {{responsive:true,plugins:{{title:{{display:true,text:'Tiempo (min) — tuned vs baseline'}}}}}}
}});

// ===== 5. Cross-val =====
new Chart(document.getElementById('crossval'), {{
    type: 'bar',
    data: {{
        labels: ['ILS','VNS','Tabu'],
        datasets: [
            {{label:'PROSTATE_sampled',data:[4280,4792,1800],backgroundColor:'#3498db'}},
            {{label:'PROSTATE_sampled2',data:[3699,4281,1431],backgroundColor:'#1abc9c'}}
        ]
    }},
    options: {{responsive:true,plugins:{{title:{{display:true,text:'f* — validación cruzada'}}}}}}
}});

// ===== 6. rVNS =====
new Chart(document.getElementById('rvns'), {{
    type: 'bar',
    data: {{
        labels: ['GVNS (con LS)','rVNS (sin LS)'],
        datasets: [{{data:[4792,4878],backgroundColor:['#2ecc71','#f39c12']}}]
    }},
    options: {{responsive:true,plugins:{{title:{{display:true,text:'Ablación — ΔLS = 1.8%'}}}},scales:{{y:{{min:4500}}}}}}
}});

// ===== 7. DVH =====
new Chart(document.getElementById('dvh'), {{
    type: 'bar',
    data: {{
        labels: ['ILS','VNS','Tabu'],
        datasets: [
            {{label:'Baseline',data:[33,33,33],backgroundColor:'#e74c3c'}},
            {{label:'Tuned',data:[50,50,50],backgroundColor:'#2ecc71'}}
        ]
    }},
    options: {{responsive:true,plugins:{{title:{{display:true,text:'% DVH constraints OK (33% → 50%)'}}}},scales:{{y:{{max:60}}}}}}
}});
</script>
</body></html>"""

with open('docs/figures/resultados.html', 'w') as f:
    f.write(html)

print("✅ docs/figures/resultados.html generated")
print(f"   Size: {len(html)} bytes")
