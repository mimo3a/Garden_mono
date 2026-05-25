package com.example.garden.model;

import com.fasterxml.jackson.annotation.JsonIgnore;
import jakarta.persistence.*;
import lombok.Getter;
import lombok.Setter;

@Entity
@Getter
@Setter
public class Sensor {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "sensor_id", nullable = false)
    private Long id;

    @Column(unique = true)
    private Integer deviceId;

    private String location;

    private String name;

    @JsonIgnore
    @OneToMany(mappedBy = "sensor", cascade = CascadeType.ALL, orphanRemoval = true)
    private java.util.List<Measurement> measurements;
}