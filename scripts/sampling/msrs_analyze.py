import argparse
import matplotlib as mpl
import numpy as np
import scipy.stats
import multiprocessing as mp

mpl.use("Agg")
import matplotlib.pyplot as plt


def setup_arg_parse():
    parser = argparse.ArgumentParser()
    parser.add_argument("sample_file", type=str,
                        help='The path to an MSR sample_file')
    parser.add_argument("-a", "--action", type=str, default="plot", help="one of: plot, corr, changes, first")
    parser.add_argument("-m", "--msr", type=str,
                        help="Specifies the hex address of the MSR for corr. E.g.: --msr=1a2b3c")
    parser.add_argument("--msrs", type=str, default="all",
                        help="Specifies the hex address of the MSRs to use the data from. Specify all for, well, all "
                             "MSRs. Examples: '--msrs=1a2b3c' '--msrs=1a2b3c,1a2b3d' '--msrs=all'")
    parser.add_argument("-s", "--similarity-threshold", type=float, default=0.75,
                        help="Specifies the minimal similarity needed to generate a plot")
    parser.add_argument("-c", "--cpus", type=str, default="all",
                        help="Specifies the CPU(s) to use the data from. CPU ids or \"all\" for, well, all CPUs. "
                             "Examples: '-c 1' '-c all' -c '5,7,9'")
    parser.add_argument("--cpu-name", type=str, default="N/A", help="Specifies the name of the CPU.")
    return parser


def parse_data_line(line):
    res = []
    msr_parts = line.split(";")
    for msr_part in msr_parts:
        res.append([int(v, 16) for v in msr_part.split(",")])
    return res


def load_msr_sample_file(path):
    # returns: a dict msrs containing a dict cpu -> list of values
    with open(path, "r") as f:
        lines = f.readlines()
    lines = list(filter(lambda l: len(l) > 0 and l[0] != '#', [l.strip() for l in lines]))

    cpus = [int(l.strip(), 16) for l in lines[0].split(",")]
    msrs = [int(l.strip(), 16) for l in lines[1].split(",")]
    time = int(lines[2], 16)

    res = {}
    for msr in msrs:
        res[msr] = {}
        for cpu in cpus:
            res[msr][cpu] = []

    for line in lines[3:]:
        parsed_line = parse_data_line(line)
        for msr_idx, msr in enumerate(msrs):
            for cpu_idx, cpu in enumerate(cpus):
                res[msr][cpu].append(parsed_line[msr_idx][cpu_idx])

    return time, res


def format_msr(msr):
    return hex(msr)[2:].rjust(8, '0')


def format_value(val):
    val = hex(val)[2:].rjust(16, '0')
    return "{}{}{}{}".format(val[:4], val[4:8], val[8:12], val[12:])


def print_values(values):
    for k, v in values.items():
        print("{} : {}".format(format_msr(k), format_value(v)))


def plot_heatmap(cpu, msr, values):
    # Heatmap of changed bits
    bits = [0] * 64
    old = bin(values[0])[2:].rjust(64, "0")[::-1]
    # for value in [bin(x)[2:].rjust(64, "0")[::-1] for x in values[1:]]:
    for value in [bin(x)[2:].rjust(64, "0")[::-1] for x in values]:
        for i, b in enumerate(value):
            if b != old[i]:
                bits[i] = bits[i] + 1

    t = [i for i in range(7, -1, -1)]

    fig, ax = plt.subplots()
    fig.set_size_inches(8.5, 8.5, forward=True)

    bit_mat = np.reshape(bits, (8, 8))

    im = ax.imshow(bit_mat, cmap="Greens")
    ax.set_yticks([])
    ax.set_xlim(-.5, 7.5)
    ax.set_ylim(-.5, 7.5)
    ax.set_xticks(t)
    ax.set_yticks(t)

    plt.title("Bitflips of the MSR {} on CPU {}".format(format_msr(msr), cpu))
    ax.set_ylabel("Byte")
    ax.set_xlabel("Bit")

    for x in range(8):
        for y in range(8):
            ax.text(x, y, bit_mat[y][x], ha="center", va="center", color="black")

    fig.tight_layout()
    plt.gca().invert_xaxis()
    # plt.gca().invert_yaxis()

    cbar = ax.figure.colorbar(im, ax=ax)
    cbar.ax.set_ylabel("Flips", rotation=-90, va="bottom")

    plt.savefig("{}_{}_hotbits.png".format(format_msr(msr), cpu))
    plt.close()


def plot_values(cpu, msr, values, scan_time):
    plt.title("MSR {} Values on CPU {}".format(format_msr(msr), cpu))
    plt.ylabel("Value")
    plt.xlabel("Second")

    plt.plot(np.linspace(0, scan_time, len(values)), values)

    plt.savefig("{}_{}_values.png".format(format_msr(msr), cpu))
    plt.close()


def calc_changes(values):
    changes = []
    old = values[0]
    for v in values[1:]:
        changes.append(v - old)
        old = v
    return changes


def calc_msr_changes(msr, values_per_cpu):
    for cpu, values in values_per_cpu.items():
        first_value = values[0]
        for value in values[1:]:
            if value != first_value:
                print("{:x}".format(msr))
                return


def plot_changes(cpu, msr, values, scan_time):
    plt.title("MSR {} Value Changes on CPU {}".format(format_msr(msr), cpu))
    plt.ylabel("Value Change")
    plt.xlabel("Second")

    changes = calc_changes(values)

    plt.plot(np.linspace(0, scan_time, len(changes)), changes)

    plt.savefig("{}_{}_changes.png".format(format_msr(msr), cpu))
    plt.close()


def plot_rchanges(cpu, msr, values, scan_time):
    plt.title("MSR {} Reverse Value Changes on CPU {}".format(format_msr(msr), cpu))
    plt.ylabel("Value Change")
    plt.xlabel("Second")

    changes = np.array(calc_changes(values)) * -1

    plt.plot(np.linspace(0, scan_time, len(changes)), changes)

    plt.savefig("{}_{}_rchanges.png".format(format_msr(msr), cpu))
    plt.close()


def plot_all_cpus(msr, cpu_values, scan_time):
    plt.title("MSR {} on all available CPUs".format(format_msr(msr)))
    plt.ylabel("Value")
    plt.xlabel("Second")
    for cpu, values in cpu_values.items():
        plt.plot(np.linspace(0, scan_time, len(values), values), values)
    plt.savefig("combined_plot_{}.png".format(format_msr(msr)))
    plt.close()


def do_plt(scan_time, cpu_values_per_msr, _):
    msr_addrs = set()
    plot_args = []

    for msr, values in cpu_values_per_msr.items():
        for cpu, vs in values.items():
            if np.sum(vs) == 0:
                continue
            msr_addrs.add(msr)
            changed = False
            old = vs[0]
            for v in vs[1:]:
                if v != old:
                    changed = True
                    break
                old = v

            if changed:
                print("MSR {} has changes on CPU {}!".format(format_msr(msr), cpu))
                plot_args.append((cpu, msr, vs, scan_time))

    with mp.Pool(mp.cpu_count()) as p:
        p.starmap(plot_values, plot_args)
        p.starmap(plot_changes, plot_args)
        p.starmap(plot_rchanges, plot_args)
        p.starmap(plot_heatmap, [(cpu, msr, vs) for cpu, msr, vs, scan_time in plot_args])
        p.starmap(plot_all_cpus, [(msr, cpu_values, scan_time) for msr, cpu_values in cpu_values_per_msr.items()])


def plot_correlation(values_a, values_b, cpu_a, cpu_b, msr_a, msr_b, scan_time, similarity_threshold, mode="unchanged"):
    x = np.linspace(0, scan_time, len(values_a))

    psim = np.nan
    ssim = np.nan

    v_a = np.asarray(values_a)
    v_b = np.asarray(values_b)

    # see https://github.com/scipy/scipy/blob/master/scipy/stats/stats.py for more information
    if np.linalg.norm(values_a - np.mean(values_a)) >= 1e-13 * np.abs(np.mean(values_a)) and np.linalg.norm(
            values_b - np.mean(values_b)) >= 1e-13 * np.abs(np.mean(values_b)) and \
            np.any(v_a != v_a[0]) and \
            np.any(v_b != v_b[0]):
        psim = scipy.stats.pearsonr(values_a, values_b)[0]
        ssim = scipy.stats.spearmanr(values_a, values_b)[0]

    res = (psim, ssim, msr_a, cpu_a, msr_b, cpu_b, mode)
    if (np.abs(psim) < similarity_threshold and np.abs(ssim) < similarity_threshold) or (
            np.isnan(psim) and np.isnan(ssim)):
        return res

    fig = plt.figure()
    fig.set_size_inches(12, 8, forward=True)
    fig.suptitle(
        "Comparison of {} on CPU {} and {} on CPU {}\n Mode: {}".format(format_msr(msr_a), cpu_a, format_msr(msr_b),
                                                                        cpu_b, mode), fontsize=15)

    grid = plt.GridSpec(3, 2, height_ratios=[3, 1, 1], figure=fig)
    ax11 = plt.subplot(grid[0, 0])
    ax12 = plt.subplot(grid[0, 1])
    ax2 = plt.subplot(grid[1, 0:])
    ax3 = plt.subplot(grid[2, 0:])

    ax11.plot(x, values_a)
    ax12.plot(x, values_b)

    ax11.set_title("{}".format(format_msr(msr_a)))
    ax12.set_title("{}".format(format_msr(msr_b)))

    ax2.set_title("Pearson Correlation Coefficient")
    ax2.barh(0, psim)
    if psim < 0:
        ax2.set_xlim(0, -1)
    else:
        ax2.set_xlim(0, 1)
    ax2.set_ylim(-.5, .5)
    ax2.set_yticks([])
    ax2.text(.5, .5, "{}".format(psim), horizontalalignment="center", verticalalignment="center",
             transform=ax2.transAxes)

    ax3.set_title("Spearman Correlation Coefficient")
    ax3.barh(0, ssim)
    if ssim < 0:
        ax3.set_xlim(0, -1)
    else:
        ax3.set_xlim(0, 1)
    ax3.set_ylim(-.5, .5)
    ax3.set_yticks([])
    ax3.text(.5, .5, "{}".format(ssim), horizontalalignment="center", verticalalignment="center",
             transform=ax3.transAxes)

    grid.tight_layout(fig, rect=[0, 0.03, 1, .95])
    fig.savefig("corr_{}_{}_{}_{}_{}.png".format(format_msr(msr_a), cpu_a, format_msr(msr_b), cpu_b, mode))
    # plt.show()
    plt.close()
    return res


def do_corr(scan_time, cpu_values_per_msr, args):
    if "msr" not in args:
        print("You need to specify an MSR with -m when performing corr!")
        exit(1)

    ref_msr = int(args["msr"], 16)

    if ref_msr not in list(cpu_values_per_msr.keys()):
        print("The MSR specified with -m is not in the dataset!")
        exit(1)

    similarity_threshold = args["similarity_threshold"]

    cpu = list(cpu_values_per_msr[list(cpu_values_per_msr.keys())[0]].keys())[0]

    calc_args = []
    for msr, values_per_cpu in cpu_values_per_msr.items():
        if msr == ref_msr:
            continue
        calc_args.append((cpu_values_per_msr[ref_msr][cpu], values_per_cpu[cpu], cpu, cpu, ref_msr, msr, scan_time,
                          similarity_threshold))
        changes = calc_changes(values_per_cpu[cpu])
        calc_args.append((cpu_values_per_msr[ref_msr][cpu][:-1], changes, cpu, cpu, ref_msr, msr, scan_time,
                          similarity_threshold, "changes"))

    with mp.Pool(mp.cpu_count()) as p:
        results = p.starmap(plot_correlation, calc_args)

    for psim, ssim, msr_a, cpu_a, msr_b, cpu_b, mode in sorted(results, key=lambda vals: np.abs(vals[0]))[::-1]:
        if not np.isnan(psim) or not np.isnan(ssim):
            print("Similarity of {} on {} and {} on {} is {} (pearson), {} (spearman) with mode {}".format(hex(msr_a),
                                                                                                           cpu_a,
                                                                                                           hex(msr_b),
                                                                                                           cpu_b,
                                                                                                           psim, ssim,
                                                                                                           mode))


def plot_non_zero_histogram(cpu, values):
    plt.title("Histogram of Static Non-Zero MSR Values on CPU {}".format(cpu))
    plt.hist([x for x in values if x != 0], bins=128)
    plt.savefig("static_nz_hist_cpu_{}.jpg".format(cpu))
    plt.close()


def print_separation():
    # print("--------------------------------------------------")
    print()


def print_zero_to_non_zero_latex_tabular(zero_non_zero_per_cpu, cpu_name):
    cpus = list(zero_non_zero_per_cpu.keys())
    cpus.sort()

    print_separation()
    print(R"""\begin{table}[H]
        \caption{Static MSRs with Value Zero Compared to Static MSRs with a Non-Zero Value on the """ + "{}".format(
        cpu_name) + R""".}
        \centering
        \label{tb:znz_""" + "{}".format(cpu_name.replace(" ", "_")) + R"""}
        \begin{tabular}{| l | c | c |}
        \hline
        \multicolumn{1}{|c|}{\textbf{CPU}} & \multicolumn{1}{p{3cm}|}{\centering \textbf{Number of} \\ \textbf{Zero MSRs}} & \multicolumn{1}{p{3cm}|}{\centering \textbf{Number of} \\ \textbf{Non-Zero MSRs}}\\
        \hline
        """)

    for cpu in cpus:
        znz = zero_non_zero_per_cpu[cpu]
        print(R"{} & {} & {} (${:0.3f}\%$)\\\hline".format(cpu, znz["zero"], znz["non_zero"], znz["non_zero_pct"]))

    print(R"""\end{tabular}\end{table}""")
    print_separation()


def print_statistical_markers_non_zero_latex_tabular(msr_values_per_cpu, cpu_name):
    cpus = list(msr_values_per_cpu.keys())
    cpus.sort()

    print_separation()
    print(R"""\begin{table}[H]
        \caption{Statistical Parameters of Static Non-Zero MSRs on the """ + "{}".format(cpu_name) + R""".}
        \centering
        \label{tb:sp_""" + "{}".format(cpu_name.replace(" ", "_")) + R"""}
        \begin{tabular}{| l | c | c | c | c |}
        \hline
        \textbf{CPU} & \textbf{Minimum} & \textbf{Maximum} & \textbf{Median} & \textbf{Modus}\\
        \hline
        """)

    for cpu in cpus:
        msr_values = msr_values_per_cpu[cpu]
        print(generate_marker_line(cpu, [x for x in msr_values.values() if x != 0]))

    print(R"\end{tabular}\end{table}")
    print_separation()


def generate_marker_line(cpu, values):
    min_str = format_value(int(np.min(values)))
    max_str = format_value(int(np.max(values)))
    median_str = format_value(int(np.median(values)))
    mode = scipy.stats.mode(values, 0)
    mode_str = format_value(int(mode.mode[0]))
    return R"{} & {} & {} & {} & {} (x{}) \\\hline".format(cpu, min_str, max_str, median_str, mode_str, mode.count[0])


def calc_zero_to_non_zero(msr_values_per_cpu):
    zero_non_zero_per_cpu = {}
    zero = "zero"
    non_zero = "non_zero"
    non_zero_pct = "non_zero_pct"
    for cpu, msr_values in msr_values_per_cpu.items():
        zero_non_zero_per_cpu[cpu] = {zero: 0, non_zero: 0, non_zero_pct: 0.0}
        for value in msr_values.values():
            if value == 0:
                zero_non_zero_per_cpu[cpu][zero] += 1
            else:
                zero_non_zero_per_cpu[cpu][non_zero] += 1
        zero_non_zero_per_cpu[cpu][non_zero_pct] = zero_non_zero_per_cpu[cpu][non_zero] / zero_non_zero_per_cpu[cpu][
            zero]

    return zero_non_zero_per_cpu


def calc_static_statistics_per_cpu(cpu, msr_values):
    values = list(msr_values.values())
    plot_non_zero_histogram(cpu, values)


def do_first(scan_time, cpu_values_per_msr, args):
    cpu_name = args["cpu_name"]
    msr_values_per_cpu = {}
    for cpu in list(cpu_values_per_msr.values())[0].keys():
        msr_values_per_cpu[cpu] = {}
    for msr, values_per_cpu in cpu_values_per_msr.items():
        for cpu, values in values_per_cpu.items():
            msr_values_per_cpu[cpu][msr] = values[0]

    for cpu, msr_values in msr_values_per_cpu.items():
        calc_static_statistics_per_cpu(cpu, msr_values)

    zero_non_zero_per_cpu = calc_zero_to_non_zero(msr_values_per_cpu)
    print_zero_to_non_zero_latex_tabular(zero_non_zero_per_cpu, cpu_name)
    print_statistical_markers_non_zero_latex_tabular(msr_values_per_cpu, cpu_name)


def do_changes(scan_time, cpu_values_per_msr, args):
    calc_args = list(cpu_values_per_msr.items())

    with mp.Pool(mp.cpu_count()) as p:
        p.starmap(calc_msr_changes, calc_args)


def main():
    # TODO (maybe)
    # Consider action "plot_all" that generates a plot for each cpu per msr regardless of present changes during the
    # scan period
    actions = {
        "plot": do_plt,
        "corr": do_corr,
        "changes": do_changes,
        "first": do_first
    }
    parser = setup_arg_parse()
    args = vars(parser.parse_args())
    if args["action"] not in actions:
        print("ERROR! This action does not exist!")
        exit(-1)

    scan_time, data = load_msr_sample_file(args["sample_file"])

    msrs_arg = args["msrs"]
    if msrs_arg != "all":
        wanted_msrs = [int(msr, 16) for msr in msrs_arg.split(",")]
        if "msr" in args and args["msr"]:
            wanted_msrs.append(int(args["msr"], 16))
        msrs = list(data.keys())
        for msr in msrs:
            if msr not in wanted_msrs:
                del data[msr]

    cpus_arg = args["cpus"]
    if cpus_arg != "all":
        to_remove = []
        wanted_cpus = [int(cpu) for cpu in cpus_arg.split(",")]
        for msr, cpu_values in data.items():
            for cpu in cpu_values.keys():
                if cpu not in wanted_cpus:
                    to_remove.append((msr, cpu))
        for msr, cpu in to_remove:
            del data[msr][cpu]

    actions[args["action"]](scan_time, data, args)


if __name__ == "__main__":
    main()
