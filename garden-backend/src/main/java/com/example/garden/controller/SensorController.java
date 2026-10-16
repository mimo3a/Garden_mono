package com.example.garden.controller;

import com.example.garden.model.Measurement;
import com.example.garden.model.Sensor;
import com.example.garden.repository.MeasurementRepository;
import com.example.garden.repository.SensorRepository;
import com.example.garden.security.AppUserPrincipal;
import com.example.garden.service.SensorAccessService;
import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/sensors")
public class SensorController {

    private final SensorRepository sensorRepository;
    private final MeasurementRepository measurementRepository;
    private final SensorAccessService sensorAccess;

    public SensorController(SensorRepository sensorRepository,
                            MeasurementRepository measurementRepository,
                            SensorAccessService sensorAccess) {
        this.sensorRepository = sensorRepository;
        this.measurementRepository = measurementRepository;
        this.sensorAccess = sensorAccess;
    }

    @GetMapping
    public List<Sensor> allSensors(@AuthenticationPrincipal AppUserPrincipal user) {
        return sensorAccess.visibleTo(user);
    }

    @GetMapping("/{deviceId}/history")
    public List<Measurement> history(@PathVariable Integer deviceId, @AuthenticationPrincipal AppUserPrincipal user) {
        Sensor sensor = sensorAccess.requireAccess(deviceId, user);

        return measurementRepository.findBySensorOrderByTimestampDesc(sensor);
    }

    @GetMapping("/{deviceId}/latest-temperature")
    public List<Measurement> latestTemperature(@PathVariable Integer deviceId, @AuthenticationPrincipal AppUserPrincipal user) {
        Sensor sensor = sensorAccess.requireAccess(deviceId, user);

        return measurementRepository
                .findTop5BySensorAndTypeOrderByTimestampDesc(sensor, "temperature");
    }

    @PutMapping("/{deviceId}")
    public Sensor updateSensor(@PathVariable Integer deviceId, @RequestBody Sensor body, @AuthenticationPrincipal AppUserPrincipal user) {
        Sensor sensor = sensorAccess.requireAccess(deviceId, user);
        if (body.getName() != null) sensor.setName(body.getName());
        if (body.getLocation() != null) sensor.setLocation(body.getLocation());
        return sensorRepository.save(sensor);
    }

    @DeleteMapping("/{deviceId}")
    public void deleteSensor(@PathVariable Integer deviceId, @AuthenticationPrincipal AppUserPrincipal user) {
        Sensor sensor = sensorAccess.requireAccess(deviceId, user);
        sensorRepository.delete(sensor);
    }
}
