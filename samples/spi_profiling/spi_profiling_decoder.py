import csv
import sys

# .csv must be provided as parameter to this script.
# .csv must contain two columns - timestamp and debug pin state.
# Debug pin state shall be captured only for changing bits.
PATH = sys.argv[1]

print(PATH)
does_memcpy = False
if "nrf54h20" in PATH:
    GPIO_TOGGLE_TIME = 5e-7 # 500ns
    does_memcpy = True
elif "nrf54l15" in PATH:
    GPIO_TOGGLE_TIME = 2e-7 # 200ns
elif "nrf52840" in PATH:
    GPIO_TOGGLE_TIME = 5e-8 # 50ns
else:
    assert("Unrecognized device")

SEGMENTS = []
SEGMENTS += [
    "Measurement start",
    "pm_device_get",
    "configuration",
    "clock request",
    "cs control",
]

if does_memcpy:
    SEGMENTS += [
        "memcpy TX",
        "cache flush",
    ]

SEGMENTS += [
    "nrfx_spim_xfer",
    "SPIM xfer + IRQ context switch",
]

if does_memcpy:
    SEGMENTS += [
        "dache invalidate",
        "memcpy RX",
    ]

SEGMENTS += [
    "in finish xfer",
    "k_sem_give",
    "in finalize xfer",
    "chip select control",
    "clock release",
    "after IRQ handling in thread context",
    "transceive() end",
]

class MeasHeaderDetector:
    def __init__(self):
        self.prev_ts_ns = 0
        self.trans_counter = 0
        self.meas_cnt = 0
        self.ts_delta_ns = 50
        self.valid_trans_ns = GPIO_TOGGLE_TIME * 1e9

    def feed(self, ts):
        ts_ns = ts * 1000000000
        diff = abs(ts_ns - self.prev_ts_ns)
        #print("diff = {}".format(diff))
        if  diff < self.valid_trans_ns + self.ts_delta_ns and \
            diff > self.valid_trans_ns - self.ts_delta_ns:
            self.trans_counter += 1
        else:
            self.trans_counter = 0
        self.prev_ts_ns = ts_ns

        if self.trans_counter == 4:
            self.trans_counter = 0
            self.meas_cnt += 1
            return True
        else:
            return False

timestamps_total = []
detector = MeasHeaderDetector()
with open(PATH) as f:
    this_timestamp_idx = 0
    this_timestamp = [0] * len(SEGMENTS)
    is_inside_meas = False
    reader = csv.reader(f)
    next(reader) # Skip header
    for line in reader:
        state = int(line[1])
        ts = float(line[0])

        is_start = detector.feed(ts)
        if is_start and not is_inside_meas:
            #print("Meas started at {}".format(ts))
            is_inside_meas = True
            this_timestamp_idx = 0
        elif is_start and is_inside_meas:
            print("Header detected inside measurement at {} !".format(ts))
            is_inside_meas = True
            this_timestamp_idx = 0
        if is_inside_meas:
            this_timestamp[this_timestamp_idx] = ts
            this_timestamp_idx += 1
            if this_timestamp_idx == len(SEGMENTS):
                #print("Meas finished at {}".format(ts))
                is_inside_meas = False
                timestamps_total.append(this_timestamp)
                this_timestamp = [0] * len(SEGMENTS)

print("Total measurements detected: {}".format(detector.meas_cnt))

# Calculate timespans for each segment
timespans_total = []
for timestamps_for_each_segment in timestamps_total:
    prev_ts = -1
    this_timespan = []
    for ts in timestamps_for_each_segment:
        if prev_ts != -1:
            # Substract time needed for GPIO toggle
            timediff = ts - prev_ts - GPIO_TOGGLE_TIME
            if timediff < 1e-8:
                # Operation might be too short to measure
                timediff = 1e-8
            this_timespan.append(timediff)
        prev_ts = ts
    timespans_total.append(this_timespan)

# Print first timespan breakdown for sanity checking
#this_timestamp = timestamps_total[0]
#this_timespan = timespans_total[0]
#for idx in range(len(this_timestamp) - 1):
#    ts1  = this_timestamp[idx]
#    ts2  = this_timestamp[idx+1]
#    diff = ts2-ts1
#    print("{:30}: {:.9f}->{:.9f} = {:.2f}".format(SEGMENTS[idx+1], ts1, ts2, diff * 1000000))

# Calculate avg value for each segment timespan
timespans_total_sum = [0] * (len(SEGMENTS) -1)
for timespans_for_each_segment in timespans_total:
    for idx, ts in enumerate(timespans_for_each_segment):
        timespans_total_sum[idx] += ts

timespans_total_avg = [0] * (len(SEGMENTS) -1)
for idx in range(len(timespans_total_sum)):
    timespans_total_avg[idx] = timespans_total_sum[idx] / detector.meas_cnt

# Print average time for each segment
print("Average timespans: (GPIO toggle time corrected)")
for idx, segment in enumerate(SEGMENTS[1:]):
    print("{} = {:.2f} [us]".format(segment, timespans_total_avg[idx] * 1000000))

print("Copy-friendly format")
for idx, segment in enumerate(SEGMENTS[1:]):
    print("{:.2f}".format(timespans_total_avg[idx] * 1000000))

# Print average transaction time
xfer_avg_timespan = 0
for idx, segment in enumerate(SEGMENTS[1:]):
    xfer_avg_timespan += timespans_total_avg[idx]
print("Average xfer time: {} [us]".format(xfer_avg_timespan * 1000000))
print("1 / measurement count = {} [us] (incl GPIO toggling and inter-transceive time)".format((1/detector.meas_cnt) * 1000000))
print("\n")
