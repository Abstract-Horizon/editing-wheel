import time


class PID:
    def __init__(self, kp: float, ki: float, kd: float, gain: float, dead_band: float) -> None:
        self.set_point = 0.0
        self.p = 0.0
        self.i = 0.0
        self.d = 0.0
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.kg = gain
        self.dead_band = dead_band
        self.last_error = 0.0
        self.last_time = 0.0
        self.last_output = 0.0
        self.last_delta = 0.0
        self.first = True

    def process(self, error: float) -> float:
        now = time.time()

        if abs(error) <= self.dead_band:
            error = 0.0

        if self.first:
            self.first = False
            self.last_error = error
            self.last_time = now
            return 0
        else:
            delta_time = now - self.last_time

            self.p = error
            if (self.last_error < 0 < error) or (self.last_error > 0 > error):
                self.i = 0.0
            elif abs(error) <= 0.1:
                self.i = 0.0
            else:
                self.i += error * delta_time

            if delta_time > 0:
                self.d = (error - self.last_error) / delta_time

            output = self.p * self.kp + self.i * self.ki + self.d * self.kd

            output *= self.kg

            self.last_output = output
            self.last_error = error
            self.last_time = now
            self.last_delta = delta_time

        return output

    def to_string(self):
        return "p=" + str(self.p * self.kp) + ", i=" + str(self.i * self.ki) + ", d=" + str(self.d * self.kd) + ", last_delta=" + str(self.last_delta)
