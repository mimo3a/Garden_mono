package com.example.garden.config;

import com.example.garden.model.AppUser;
import com.example.garden.model.Role;
import com.example.garden.repository.AppUserRepository;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.CommandLineRunner;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.crypto.password.PasswordEncoder;

@Configuration
public class BootstrapAdmin {
    @Bean
    CommandLineRunner createInitialAdmin(AppUserRepository users, PasswordEncoder encoder,
                                         @Value("${app.bootstrap-admin.email:}") String email,
                                         @Value("${app.bootstrap-admin.password:}") String password) {
        return args -> {
            if (users.existsByRole(Role.ADMIN) || email.isBlank() || password.isBlank()) return;
            AppUser admin = new AppUser();
            admin.setEmail(email.trim().toLowerCase());
            admin.setPasswordHash(encoder.encode(password));
            admin.setRole(Role.ADMIN);
            users.save(admin);
        };
    }
}
