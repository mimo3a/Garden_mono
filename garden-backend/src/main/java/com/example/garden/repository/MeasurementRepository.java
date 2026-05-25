package com.example.garden.repository;

import com.example.garden.model.Measurement;
import com.example.garden.model.Sensor;
import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;


public interface MeasurementRepository extends JpaRepository<Measurement, Long> {
     List<Measurement> findBySensorOrderByTimestampDesc(Sensor sensor);

    List<Measurement> findTop5BySensorAndTypeOrderByTimestampDesc(Sensor sensor, String type);
}
