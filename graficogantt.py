import pandas as pd
import matplotlib.pyplot as plt
import sys
import random # Para cores aleatórias

def plot_gantt(csv_filename):
    try:
        df = pd.read_csv(csv_filename)
    except FileNotFoundError:
        print(f"Erro: Ficheiro '{csv_filename}' não encontrado.")
        return
    except Exception as e:
        print(f"Erro ao ler o CSV: {e}")
        return

    if df.empty:
        print("Ficheiro CSV está vazio. Nada para plotar.")
        return

    # Cores para os processos (e uma para idle)
    pids = sorted(df['PID'].unique())
    colors = {}
    num_colors = len(pids)
    # Gerador de cores aleatórias distintas (exceto para IDLE)
    color_map = plt.cm.get_cmap('viridis', num_colors) # Ou 'tab20', 'hsv', etc.
    color_index = 0
    for pid in pids:
        if pid == -1:
            colors[pid] = 'grey' # Cinza para IDLE
        # elif pid == -2: # Se decidir logar CS
        #     colors[pid] = 'red'
        else:
             # Evita cores muito claras ou muito escuras se possível
             rgb = color_map(color_index / max(1, num_colors -1)) # Normaliza índice
             colors[pid] = rgb
             color_index += 1


    fig, ax = plt.subplots(1, 1, figsize=(12, max(4, len(pids) * 0.5))) # Ajusta altura

    # Plota barras horizontais para cada segmento
    for index, row in df.iterrows():
        pid = row['PID']
        start = row['Start']
        end = row['End']
        duration = end - start
        color = colors.get(pid, 'black') # Preto se PID inesperado

        label = f"P{pid}" if pid >= 0 else ("IDLE" if pid == -1 else f"Estado {pid}")

        # Usar o ID do processo (ou uma representação de IDLE) como posição Y
        y_pos = pids.index(pid)

        ax.barh(y=y_pos, width=duration, left=start, height=0.8,
                color=color, alpha=0.8, edgecolor='black')
        # Adiciona texto no meio da barra (se for longa o suficiente)
        if duration > 0.5 : # Ajuste conforme necessário
             ax.text(start + duration / 2, y_pos, label,
                     ha='center', va='center', color='white', fontsize=8,
                     fontweight='bold')

    # Configurações do gráfico
    ax.set_yticks(range(len(pids)))
    ax.set_yticklabels([f"P{pid}" if pid >=0 else ("IDLE" if pid == -1 else f"E{pid}") for pid in pids])
    ax.set_xlabel("Tempo")
    ax.set_ylabel("Processo / Estado")
    plt.title(f"Gráfico de Gantt - Simulação ({csv_filename})")
    plt.grid(axis='x', linestyle='--', alpha=0.6)

    # Ajusta limites do eixo X
    if not df.empty:
         ax.set_xlim(0, df['End'].max() * 1.05) # Um pouco de margem

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python plot_gantt.py gantt_data_fcfs.csv")
    else:
        plot_gantt(sys.argv[1])
