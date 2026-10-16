package com.example.garden.service;

import com.example.garden.model.Sensor;
import com.example.garden.repository.SensorRepository;
import com.example.garden.security.AppUserPrincipal;
import org.springframework.security.access.AccessDeniedException;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class SensorAccessService {
    private final SensorRepository sensors;
    public SensorAccessService(SensorRepository sensors) { this.sensors = sensors; }

    public List<Sensor> visibleTo(AppUserPrincipal user) {
        return "ADMIN".equals(user.role()) ? sensors.findAll() : sensors.findByOwnerId(user.id());
    }

    public Sensor requireAccess(Integer deviceId, AppUserPrincipal user) {
        if ("ADMIN".equals(user.role())) return sensors.findByDeviceId(deviceId).orElseThrow(() -> new IllegalArgumentException("Sensor not found"));
        return sensors.findByDeviceIdAndOwnerId(deviceId, user.id())
                .orElseThrow(() -> new AccessDeniedException("You do not have access to this sensor"));
    }
}
