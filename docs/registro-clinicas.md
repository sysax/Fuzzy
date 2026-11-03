# Registro de clínicas

Registro operativo de las sedes dadas de alta. Cada fila la firma el operador
que ejecutó el alta y la confirma la sede. No se anotan tokens, contraseñas ni
datos personales de clientes.

## Producción

| Sede | `slug` | Grupo Turso | Bases | Esquema | Alta | Responsable | Respaldo | Próximo simulacro |
|---|---|---|---|---|---|---|---|---|
| _Sin sedes de producción registradas todavía._ | | | | | | | | |

## Staging

| Sede | `slug` | Grupo Turso | Uso | Responsable |
|---|---|---|---|---|
| Clínica Veterinaria Staging 1 | `staging-1` | `fuzzy-clinic-staging-1` | Ensayo de runbooks y migraciones | Equipo FuzzyCloud |
| Clínica Veterinaria Staging 2 | `staging-2` | `fuzzy-clinic-staging-2` | Ensayo de runbooks y migraciones | Equipo FuzzyCloud |

## Cómo registrar una sede

1. Ejecutar el alta de [`produccion.md`](produccion.md) §4.
2. Completar la hoja de evidencia de [`runbook-master.md`](runbook-master.md) §9.
3. Añadir la fila con: nombre, `slug`, grupo, bases (`clinic`/`inbox`), versión
   de esquema, fecha de alta, responsable, frecuencia y destino de respaldo, y
   fecha del próximo simulacro.
4. No escribir tokens ni credenciales. Si hace falta referenciar un secreto,
   usar el nombre del gestor y la clave, nunca el valor.

## Cómo dar de baja una sede

1. Ejecutar [`runbook-master.md`](runbook-master.md) §8.
2. Marcar la fila como `baja` con fecha y motivo.
3. Conservar la fila; no borrar el historial.
