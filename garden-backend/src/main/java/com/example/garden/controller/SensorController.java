package com.example.garden.controller;

import com.example.garden.model.Measurement;
import com.example.garden.model.Sensor;
import com.example.garden.repository.MeasurementRepository;
import com.example.garden.repository.SensorRepository;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/sensors")
public class SensorController {

    private final SensorRepository sensorRepository;
    private final MeasurementRepository measurementRepository;

    public SensorController(SensorRepository sensorRepository,
                            MeasurementRepository measurementRepository) {
        this.sensorRepository = sensorRepository;
        this.measurementRepository = measurementRepository;
    }

    @GetMapping
    public List<Sensor> allSensors() {
        return sensorRepository.findAll();
    }

    @GetMapping("/{deviceId}/history")
    public List<Measurement> history(@PathVariable Integer deviceId) {

        Sensor sensor = sensorRepository.findByDeviceId(deviceId)
                .orElseThrow(() -> new RuntimeException("Sensor not found"));

        return measurementRepository.findBySensorOrderByTimestampDesc(sensor);
    }

    @GetMapping("/{deviceId}/latest-temperature")
    public List<Measurement> latestTemperature(@PathVariable Integer deviceId) {

        Sensor sensor = sensorRepository.findByDeviceId(deviceId)
                .orElseThrow(() -> new RuntimeException("Sensor not found"));

        return measurementRepository
                .findTop5BySensorAndTypeOrderByTimestampDesc(sensor, "temperature");
    }

    @PutMapping("/{deviceId}")
    public Sensor updateSensor(@PathVariable Integer deviceId, @RequestBody Sensor body) {
        Sensor sensor = sensorRepository.findByDeviceId(deviceId)
                .orElseThrow(() -> new RuntimeException("Sensor not found"));
        if (body.getName() != null) sensor.setName(body.getName());
        if (body.getLocation() != null) sensor.setLocation(body.getLocation());
        return sensorRepository.save(sensor);
    }

    @DeleteMapping("/{deviceId}")
    public void deleteSensor(@PathVariable Integer deviceId) {
        Sensor sensor = sensorRepository.findByDeviceId(deviceId)
                .orElseThrow(() -> new RuntimeException("Sensor not found"));
        sensorRepository.delete(sensor);
    }
}