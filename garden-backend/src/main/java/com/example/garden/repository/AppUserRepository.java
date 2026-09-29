package com.example.garden.repository;

import com.example.garden.model.AppUser;
import com.example.garden.model.Role;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.Optional;

public interface AppUserRepository extends JpaRepository<AppUser, Long> {
    Optional<AppUser> findByEmailIgnoreCase(String email);
    boolean existsByRole(Role role);
}
