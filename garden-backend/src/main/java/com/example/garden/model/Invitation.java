package com.example.garden.model;

import jakarta.persistence.*;
import lombok.Getter;
import lombok.Setter;

import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;

@Entity
@Getter
@Setter
public class Invitation {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true, length = 64)
    private String token;

    @Column(nullable = false)
    private String email;

    @Column(nullable = false)
    private LocalDateTime expiresAt;

    private LocalDateTime acceptedAt;

    @ElementCollection
    @CollectionTable(name = "invitation_device_ids", joinColumns = @JoinColumn(name = "invitation_id"))
    @Column(name = "device_id", nullable = false)
    private List<Integer> deviceIds = new ArrayList<>();
}
