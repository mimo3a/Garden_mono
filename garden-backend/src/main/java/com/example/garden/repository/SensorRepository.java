package com.example.garden.repository;

import com.example.garden.model.Sensor;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.Optional;
import java.util.List;

public interface SensorRepository extends JpaRepository<Sensor, Long> {
    Optional<Sensor> findByDeviceId(Integer deviceId);
    Optional<Sensor> findByDeviceIdAndOwnerId(Integer deviceId, Long ownerId);
    List<Sensor> findByOwnerId(Long ownerId);
}
