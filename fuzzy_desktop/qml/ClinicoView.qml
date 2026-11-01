import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Flujo clínico de la fase 9: recepción, consulta, laboratorio,
// hospitalización, cirugía, estética, caja e inventario. La vista solo
// presenta datos y llama al controlador; el RBAC y las transacciones viven en
// fuzzy::core::ServicioClinico.
Item {
    id: root
    objectName: "clinicoView"

    property string seccion: "recepcion"
    property string mascotaActual: ""
    property string consultaActual: ""
    property string citaActual: ""
    property string laboratorioActual: ""
    property string hospitalizacionActual: ""
    property string cirugiaActual: ""
    property string cargoActual: ""
    property string conceptoActual: ""
    property string modoCatalogo: "servicio"

    readonly property var secciones: [
        { clave: "recepcion", nombre: qsTr("Recepción") },
        { clave: "consultas", nombre: qsTr("Consultas") },
        { clave: "laboratorio", nombre: qsTr("Laboratorio") },
        { clave: "hospital", nombre: qsTr("Hospitalización") },
        { clave: "cirugia", nombre: qsTr("Cirugía") },
        { clave: "estetica", nombre: qsTr("Estética") },
        { clave: "caja", nombre: qsTr("Caja") },
        { clave: "catalogos", nombre: qsTr("Catálogos") }
    ]

    readonly property bool puedeConsulta: sesion.permisos.indexOf("RegistrarConsulta") !== -1
    readonly property bool puedeFactura: sesion.permisos.indexOf("EmitirFactura") !== -1
    readonly property bool puedeCaja: sesion.permisos.indexOf("CerrarCaja") !== -1
    readonly property bool puedeEstetica: sesion.permisos.indexOf("RegistrarEstetica") !== -1
    readonly property bool puedeCatalogo: sesion.permisos.indexOf("ConfigurarClinica") !== -1

    function indiceDe(lista, uuid) {
        for (var i = 0; i < lista.length; ++i) {
            if (lista[i].uuid === uuid)
                return i
        }
        return -1
    }

    function abrirTriaje(mascotaUuid, consultaUuid) {
        clinico.limpiarMensajes()
        root.mascotaActual = mascotaUuid
        root.consultaActual = consultaUuid
        comboMascotaTriaje.currentIndex = indiceDe(clinico.mascotas, mascotaUuid)
        triajePeso.text = ""
        triajeTemperatura.text = ""
        triajeCardiaca.text = ""
        triajeRespiratoria.text = ""
        triajeNotas.text = ""
        dialogoTriaje.open()
    }

    function abrirConsulta(mascotaUuid, citaUuid) {
        clinico.limpiarMensajes()
        root.mascotaActual = mascotaUuid
        root.citaActual = citaUuid
        comboMascotaConsulta.currentIndex = indiceDe(clinico.mascotas, mascotaUuid)
        consultaMotivo.text = ""
        consultaEmergencia.checked = false
        dialogoConsulta.open()
    }

    function abrirCierreConsulta(consultaUuid) {
        clinico.limpiarMensajes()
        root.consultaActual = consultaUuid
        cierreDiagnostico.text = ""
        cierreTratamiento.text = ""
        cierreExamen.text = ""
        dialogoCerrarConsulta.open()
    }

    function abrirReceta(consultaUuid) {
        clinico.limpiarMensajes()
        root.consultaActual = consultaUuid
        comboConsultaReceta.currentIndex = indiceDe(clinico.consultasAbiertas, consultaUuid)
        recetaIndicaciones.text = ""
        recetaProducto.currentIndex = -1
        recetaMedicamento.text = ""
        recetaDosis.text = ""
        recetaFrecuencia.text = ""
        recetaDuracion.text = ""
        recetaCantidad.text = "1"
        recetaPrecio.text = "0"
        modeloReceta.clear()
        dialogoReceta.open()
    }

    function abrirCargo(origenTipo, origenUuid, concepto, monto) {
        clinico.limpiarMensajes()
        cargoOrigenTipo.currentIndex = Math.max(0, cargoOrigenTipo.model.indexOf(origenTipo))
        cargoOrigenUuid.text = origenUuid
        cargoConcepto.text = concepto
        cargoMonto.text = String(monto)
        dialogoCargo.open()
    }

    function abrirFactura(cargo) {
        clinico.limpiarMensajes()
        root.cargoActual = cargo.uuid
        root.conceptoActual = cargo.concepto
        facturaMonto.text = String(cargo.monto)
        facturaDescuento.text = "0"
        facturaImpuesto.text = "0"
        facturaMetodo.currentIndex = 0
        dialogoFacturar.open()
    }

    Rectangle {
        anchors.fill: parent
        color: estilo.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        PageHeader {
            Layout.fillWidth: true
            titulo: qsTr("Flujo clínico")
            subtitulo: qsTr("%1 citas hoy · %2 consultas abiertas · %3 exámenes pendientes")
                          .arg(clinico.resumen.citasHoy || 0)
                          .arg(clinico.resumen.consultasAbiertas || 0)
                          .arg(clinico.resumen.laboratorioPendiente || 0)

            SecondaryButton {
                text: qsTr("Solicitar laboratorio")
                visible: root.puedeConsulta
                onClicked: {
                    clinico.limpiarMensajes()
                    comboMascotaLab.currentIndex = 0
                    comboConsultaLab.currentIndex = -1
                    comboExamenLab.currentIndex = 0
                    labPrioridad.currentIndex = 0
                    labNotas.text = ""
                    dialogoLaboratorio.open()
                }
            }
            PrimaryButton {
                text: qsTr("Nueva consulta")
                visible: root.puedeConsulta
                onClicked: root.abrirConsulta("", "")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            KpiCard { titulo: qsTr("En espera"); valor: clinico.resumen.enEspera || 0; subtexto: qsTr("recepción y triaje"); icono: "hourglass-split" }
            KpiCard { titulo: qsTr("Hospitalizados"); valor: clinico.resumen.hospitalizados || 0; subtexto: qsTr("pacientes activos"); icono: "file-medical" }
            KpiCard { titulo: qsTr("Cirugías"); valor: clinico.resumen.cirugiasProgramadas || 0; subtexto: qsTr("programadas o en curso"); icono: "capsule" }
            KpiCard { titulo: qsTr("Cargos pendientes"); valor: clinico.resumen.cargosPendientes || 0; subtexto: qsTr("listos para facturar"); icono: "receipt" }
        }

        // ── Pestañas ─────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: root.secciones

                delegate: Rectangle {
                    required property var modelData
                    width: Math.max(96, etiqueta.implicitWidth + 28)
                    height: 36
                    radius: estilo.radiusPill
                    color: root.seccion === modelData.clave ? estilo.ink : estilo.surface
                    border.width: 1
                    border.color: root.seccion === modelData.clave ? estilo.ink : estilo.border

                    Label {
                        id: etiqueta
                        anchors.centerIn: parent
                        text: modelData.nombre
                        font.pixelSize: 12
                        color: root.seccion === modelData.clave ? "#FFFFFF" : estilo.textSecondary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.seccion = modelData.clave
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 38 : 0
            visible: clinico.error.length > 0 || clinico.mensaje.length > 0
            radius: 10
            color: clinico.error.length > 0 ? estilo.dangerBg : estilo.successBg
            border.width: 1
            border.color: clinico.error.length > 0 ? "#F7C9CB" : "#CDEBD7"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: clinico.error.length > 0 ? clinico.error : clinico.mensaje
                    color: clinico.error.length > 0 ? estilo.danger : estilo.success
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Label {
                    text: "×"
                    color: clinico.error.length > 0 ? estilo.danger : estilo.success
                    font.pixelSize: 18
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: clinico.limpiarMensajes()
                    }
                }
            }
        }

        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            margen: 0

            // ── Recepción y triaje ───────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "recepcion"

                EncabezadoTabla {
                    Layout.fillWidth: true
                    columnas: [qsTr("HORA"), qsTr("PACIENTE"), qsTr("PROPIETARIO"),
                               qsTr("SERVICIO"), qsTr("ESTADO"), qsTr("ACCIONES")]
                    anchos: [70, 210, 220, 150, 110, 250]
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.agendaDelDia.length === 0
                    icono: "calendar3"
                    titulo: qsTr("Sin citas para hoy")
                    detalle: qsTr("Agenda una cita desde la vista Agenda para iniciar el flujo.")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.agendaDelDia.length > 0
                    clip: true
                    model: clinico.agendaDelDia

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 58

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            Label { Layout.preferredWidth: 70; text: modelData.hora; font.pixelSize: 12; font.bold: true; color: estilo.ink }
                            ColumnLayout {
                                Layout.preferredWidth: 210
                                spacing: 0
                                Label { text: modelData.mascota; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: modelData.tipo; font.pixelSize: 10; color: estilo.textMuted }
                            }
                            Label { Layout.preferredWidth: 220; text: modelData.propietario; font.pixelSize: 12; color: estilo.textSecondary; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 150; text: modelData.emergencia ? qsTr("Emergencia") : qsTr("Mostrador"); font.pixelSize: 12; color: modelData.emergencia ? estilo.danger : estilo.textSecondary }
                            StatusChip { Layout.preferredWidth: 110; estado: modelData.estado }
                            RowLayout {
                                Layout.preferredWidth: 250
                                spacing: 6

                                SecondaryButton {
                                    visible: !modelData.llegada
                                    text: qsTr("Recibir")
                                    onClicked: clinico.recibir(modelData.uuid)
                                }
                                SecondaryButton {
                                    visible: modelData.llegada
                                    text: qsTr("Triaje")
                                    onClicked: root.abrirTriaje(modelData.mascotaUuid, "")
                                }
                                PrimaryButton {
                                    visible: modelData.llegada && root.puedeConsulta
                                    text: qsTr("Consulta")
                                    onClicked: root.abrirConsulta(modelData.mascotaUuid, modelData.uuid)
                                }
                            }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Consultas abiertas ───────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "consultas"

                EncabezadoTabla {
                    Layout.fillWidth: true
                    columnas: [qsTr("PACIENTE"), qsTr("PROPIETARIO"), qsTr("MOTIVO"),
                               qsTr("INICIO"), qsTr("ACCIONES")]
                    anchos: [220, 220, 300, 130, 310]
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.consultasAbiertas.length === 0
                    icono: "file-medical"
                    titulo: qsTr("Sin consultas abiertas")
                    detalle: qsTr("Recibe una cita y abre la consulta para registrar diagnóstico y receta.")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.consultasAbiertas.length > 0
                    clip: true
                    model: clinico.consultasAbiertas

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 58

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            Label { Layout.preferredWidth: 220; text: modelData.mascota; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 220; text: modelData.propietario; font.pixelSize: 12; color: estilo.textSecondary; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 300; text: modelData.motivo; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 130; text: modelData.fecha; font.pixelSize: 11; color: estilo.textMuted }
                            RowLayout {
                                Layout.preferredWidth: 310
                                spacing: 6
                                SecondaryButton {
                                    text: qsTr("Cargo")
                                    onClicked: root.abrirCargo("Consulta", modelData.uuid,
                                                               qsTr("Consulta general"), 85000)
                                }
                                SecondaryButton {
                                    text: qsTr("Receta")
                                    onClicked: root.abrirReceta(modelData.uuid)
                                }
                                PrimaryButton {
                                    text: qsTr("Cerrar")
                                    onClicked: root.abrirCierreConsulta(modelData.uuid)
                                }
                            }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Laboratorio ──────────────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "laboratorio"

                EncabezadoTabla {
                    Layout.fillWidth: true
                    columnas: [qsTr("PACIENTE"), qsTr("EXAMEN"), qsTr("PRIORIDAD"),
                               qsTr("ESTADO"), qsTr("RESULTADO"), qsTr("ACCIONES")]
                    anchos: [200, 210, 100, 110, 260, 230]
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.laboratorio.length === 0
                    icono: "capsule"
                    titulo: qsTr("Sin solicitudes de laboratorio")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.laboratorio.length > 0
                    clip: true
                    model: clinico.laboratorio

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 62

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            ColumnLayout {
                                Layout.preferredWidth: 200
                                spacing: 0
                                Label { text: modelData.mascota; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: modelData.propietario; font.pixelSize: 10; color: estilo.textMuted; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            Label { Layout.preferredWidth: 210; text: modelData.examen; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 100; text: modelData.prioridad; font.pixelSize: 11; color: modelData.prioridad === "Urgente" ? estilo.danger : estilo.textSecondary }
                            StatusChip { Layout.preferredWidth: 110; estado: modelData.estado }
                            Label { Layout.preferredWidth: 260; text: modelData.resultado || qsTr("Pendiente"); font.pixelSize: 11; color: estilo.textSecondary; elide: Text.ElideRight }
                            RowLayout {
                                Layout.preferredWidth: 230
                                spacing: 6
                                SecondaryButton {
                                    visible: modelData.estado === "Solicitado"
                                    text: qsTr("Iniciar")
                                    onClicked: clinico.iniciarLaboratorio(modelData.uuid)
                                }
                                SecondaryButton {
                                    visible: modelData.estado === "En proceso"
                                    text: qsTr("Resultado")
                                    onClicked: {
                                        clinico.limpiarMensajes()
                                        root.laboratorioActual = modelData.uuid
                                        resultadoTexto.text = modelData.resultado || ""
                                        dialogoResultado.open()
                                    }
                                }
                                PrimaryButton {
                                    visible: modelData.estado === "Listo"
                                    text: qsTr("Entregar")
                                    onClicked: clinico.entregarLaboratorio(modelData.uuid)
                                }
                            }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Hospitalización ──────────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "hospital"

                EncabezadoTabla {
                    Layout.fillWidth: true
                    columnas: [qsTr("PACIENTE"), qsTr("MOTIVO"), qsTr("ÁREA"),
                               qsTr("INGRESO"), qsTr("ESTADO"), qsTr("ACCIONES")]
                    anchos: [200, 300, 120, 160, 110, 250]
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.hospitalizaciones.length === 0
                    icono: "file-medical"
                    titulo: qsTr("Sin pacientes hospitalizados")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.hospitalizaciones.length > 0
                    clip: true
                    model: clinico.hospitalizaciones

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 62

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            ColumnLayout {
                                Layout.preferredWidth: 200
                                spacing: 0
                                Label { text: modelData.mascota; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: modelData.propietario; font.pixelSize: 10; color: estilo.textMuted; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            Label { Layout.preferredWidth: 300; text: modelData.motivo; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 120; text: modelData.area || "—"; font.pixelSize: 11; color: estilo.textSecondary }
                            Label { Layout.preferredWidth: 160; text: modelData.ingreso; font.pixelSize: 11; color: estilo.textMuted }
                            StatusChip { Layout.preferredWidth: 110; estado: modelData.estado }
                            RowLayout {
                                Layout.preferredWidth: 250
                                spacing: 6
                                SecondaryButton {
                                    visible: modelData.estado === "Activa"
                                    text: qsTr("Evolución")
                                    onClicked: {
                                        clinico.limpiarMensajes()
                                        root.hospitalizacionActual = modelData.uuid
                                        evolucionNota.text = ""
                                        evolucionPeso.text = ""
                                        evolucionTemperatura.text = ""
                                        evolucionCardiaca.text = ""
                                        dialogoEvolucion.open()
                                    }
                                }
                                PrimaryButton {
                                    visible: modelData.estado === "Activa"
                                    text: qsTr("Alta")
                                    onClicked: {
                                        clinico.limpiarMensajes()
                                        root.hospitalizacionActual = modelData.uuid
                                        altaEstado.currentIndex = 0
                                        dialogoAlta.open()
                                    }
                                }
                            }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Cirugía ──────────────────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "cirugia"

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    Layout.topMargin: 10
                    Layout.bottomMargin: 6

                    Label { Layout.fillWidth: true; text: qsTr("QUIRÓFANO"); font.pixelSize: 11; font.bold: true; color: estilo.ink }
                    SecondaryButton {
                        text: qsTr("Programar cirugía")
                        onClicked: {
                            clinico.limpiarMensajes()
                            comboMascotaCirugia.currentIndex = 0
                            cirugiaProcedimiento.text = ""
                            cirugiaFecha.text = ""
                            cirugiaAnestesia.text = ""
                            dialogoCirugia.open()
                        }
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.cirugias.length === 0
                    icono: "capsule"
                    titulo: qsTr("Sin cirugías programadas")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.cirugias.length > 0
                    clip: true
                    model: clinico.cirugias

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 62

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            ColumnLayout {
                                Layout.preferredWidth: 200
                                spacing: 0
                                Label { text: modelData.mascota; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: modelData.propietario; font.pixelSize: 10; color: estilo.textMuted; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            ColumnLayout {
                                Layout.preferredWidth: 300
                                spacing: 0
                                Label { text: modelData.procedimiento; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: modelData.anestesia || qsTr("Sin anestesia indicada"); font.pixelSize: 10; color: estilo.textMuted }
                            }
                            Label { Layout.preferredWidth: 170; text: modelData.fecha; font.pixelSize: 11; color: estilo.textMuted }
                            StatusChip { Layout.preferredWidth: 110; estado: modelData.estado }
                            RowLayout {
                                Layout.preferredWidth: 230
                                spacing: 6
                                SecondaryButton {
                                    visible: modelData.estado === "Programada"
                                    text: qsTr("Iniciar")
                                    onClicked: clinico.iniciarCirugia(modelData.uuid)
                                }
                                PrimaryButton {
                                    visible: modelData.estado === "En curso"
                                    text: qsTr("Cerrar")
                                    onClicked: {
                                        clinico.limpiarMensajes()
                                        root.cirugiaActual = modelData.uuid
                                        cirugiaDiagnostico.text = ""
                                        cirugiaNotas.text = ""
                                        dialogoCerrarCirugia.open()
                                    }
                                }
                            }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Estética ─────────────────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "estetica"

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    Layout.topMargin: 10
                    Layout.bottomMargin: 6

                    Label { Layout.fillWidth: true; text: qsTr("SERVICIOS DE ESTÉTICA"); font.pixelSize: 11; font.bold: true; color: estilo.ink }
                    PrimaryButton {
                        visible: root.puedeEstetica
                        text: qsTr("Registrar servicio")
                        onClicked: {
                            clinico.limpiarMensajes()
                            comboMascotaEstetica.currentIndex = 0
                            comboEsteticaTipo.currentIndex = 0
                            esteticaPrecio.text = "0"
                            esteticaObservaciones.text = ""
                            esteticaCobrar.checked = true
                            dialogoEstetica.open()
                        }
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.estetica.length === 0
                    icono: "people"
                    titulo: qsTr("Sin servicios de estética registrados")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.estetica.length > 0
                    clip: true
                    model: clinico.estetica

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 58

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            Label { Layout.preferredWidth: 200; text: modelData.mascota; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 170; text: modelData.tipo; font.pixelSize: 12; color: estilo.ink }
                            Label { Layout.fillWidth: true; text: modelData.observaciones || "—"; font.pixelSize: 11; color: estilo.textSecondary; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 110; text: modelData.precioTexto; font.pixelSize: 12; font.bold: true; color: estilo.ink }
                            Label { Layout.preferredWidth: 150; text: modelData.fecha; font.pixelSize: 11; color: estilo.textMuted }
                            StatusChip { Layout.preferredWidth: 100; estado: modelData.estado }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Caja ─────────────────────────────────────────
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: root.seccion === "caja"

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    Layout.topMargin: 10
                    Layout.bottomMargin: 6

                    Label { Layout.fillWidth: true; text: qsTr("CARGOS PENDIENTES"); font.pixelSize: 11; font.bold: true; color: estilo.ink }
                    SecondaryButton {
                        visible: root.puedeCaja
                        text: qsTr("Cerrar caja")
                        onClicked: {
                            clinico.limpiarMensajes()
                            cajaReal.text = "0"
                            cajaObservaciones.text = ""
                            dialogoCaja.open()
                        }
                    }
                }

                EncabezadoTabla {
                    Layout.fillWidth: true
                    columnas: [qsTr("CONCEPTO"), qsTr("PACIENTE"), qsTr("PROPIETARIO"),
                               qsTr("MONTO"), qsTr("ORIGEN"), qsTr("")]
                    anchos: [300, 200, 220, 120, 120, 120]
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.cargos.length === 0
                    icono: "receipt"
                    titulo: qsTr("Sin cargos pendientes")
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: clinico.cargos.length > 0
                    clip: true
                    model: clinico.cargos

                    delegate: Item {
                        required property var modelData
                        width: ListView.view.width
                        height: 54

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            Label { Layout.preferredWidth: 300; text: modelData.concepto; font.pixelSize: 12; font.bold: true; color: estilo.ink; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 200; text: modelData.mascota || "—"; font.pixelSize: 12; color: estilo.textSecondary; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 220; text: modelData.propietario || "—"; font.pixelSize: 12; color: estilo.textSecondary; elide: Text.ElideRight }
                            Label { Layout.preferredWidth: 120; text: "$" + Number(modelData.monto).toFixed(2); font.pixelSize: 12; font.bold: true; color: estilo.ink }
                            Label { Layout.preferredWidth: 120; text: modelData.origenTipo; font.pixelSize: 11; color: estilo.textMuted }
                            PrimaryButton {
                                Layout.preferredWidth: 120
                                visible: root.puedeFactura
                                text: qsTr("Facturar")
                                onClicked: root.abrirFactura(modelData)
                            }
                        }

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                    }
                }
            }

            // ── Catálogos ────────────────────────────────────
            RowLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14
                visible: root.seccion === "catalogos"

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    titulo: qsTr("Servicios")
                    subtitulo: qsTr("Precios que usa la facturación")

                    SecondaryButton {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        visible: root.puedeCatalogo
                        text: qsTr("Nuevo")
                        onClicked: {
                            clinico.limpiarMensajes()
                            root.modoCatalogo = "servicio"
                            catalogoCodigo.text = ""
                            catalogoNombre.text = ""
                            catalogoPrecio.text = "0"
                            catalogoActivo.checked = true
                            dialogoCatalogo.open()
                        }
                    }

                    ListView {
                        anchors.fill: parent
                        anchors.topMargin: 46
                        clip: true
                        model: clinico.servicios

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 46

                            RowLayout {
                                anchors.fill: parent
                                spacing: 8
                                Label { Layout.preferredWidth: 90; text: modelData.codigo; font.pixelSize: 11; color: estilo.textMuted }
                                Label { Layout.fillWidth: true; text: modelData.nombre; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                                Label { text: "$" + Number(modelData.precio).toFixed(2); font.pixelSize: 12; font.bold: true; color: estilo.ink }
                                StatusChip { estado: modelData.activo ? "Activo" : "Cancelada" }
                            }

                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                        }
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    titulo: qsTr("Exámenes de laboratorio")
                    subtitulo: qsTr("Catálogo del nodo")

                    SecondaryButton {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        visible: root.puedeCatalogo
                        text: qsTr("Nuevo")
                        onClicked: {
                            clinico.limpiarMensajes()
                            root.modoCatalogo = "examen"
                            catalogoCodigo.text = ""
                            catalogoNombre.text = ""
                            catalogoPrecio.text = "0"
                            catalogoActivo.checked = true
                            dialogoCatalogo.open()
                        }
                    }

                    ListView {
                        anchors.fill: parent
                        anchors.topMargin: 46
                        clip: true
                        model: clinico.examenes

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 46

                            RowLayout {
                                anchors.fill: parent
                                spacing: 8
                                Label { Layout.preferredWidth: 90; text: modelData.codigo; font.pixelSize: 11; color: estilo.textMuted }
                                Label { Layout.fillWidth: true; text: modelData.nombre; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                                StatusChip { estado: modelData.activo ? "Activo" : "Cancelada" }
                            }

                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: estilo.border }
                        }
                    }
                }
            }
        }
    }

    // ── Diálogos ─────────────────────────────────────────────
    AppDialog {
        id: dialogoTriaje
        titulo: qsTr("Triaje")
        descripcion: qsTr("Registra los signos vitales antes de la consulta.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: comboMascotaTriaje
                Layout.fillWidth: true
                model: clinico.mascotas
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige un paciente")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                LabeledField { id: triajePeso; Layout.fillWidth: true; label: qsTr("Peso (kg)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: triajeTemperatura; Layout.fillWidth: true; label: qsTr("Temperatura (°C)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: triajeCardiaca; Layout.fillWidth: true; label: qsTr("FC (lpm)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: triajeRespiratoria; Layout.fillWidth: true; label: qsTr("FR (rpm)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }

            LabeledField { id: triajeNotas; Layout.fillWidth: true; label: qsTr("Notas de recepción"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoTriaje.close() },
            PrimaryButton {
                text: qsTr("Registrar")
                onClicked: {
                    clinico.registrarTriaje(comboMascotaTriaje.currentIndex >= 0
                                            ? clinico.mascotas[comboMascotaTriaje.currentIndex].uuid
                                            : "",
                                            root.consultaActual,
                                            Number(triajePeso.text),
                                            Number(triajeTemperatura.text),
                                            Number(triajeCardiaca.text),
                                            Number(triajeRespiratoria.text),
                                            triajeNotas.text)
                    if (clinico.error.length === 0)
                        dialogoTriaje.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoConsulta
        titulo: qsTr("Nueva consulta")
        descripcion: qsTr("El veterinario firma la consulta al cerrarla con diagnóstico.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: comboMascotaConsulta
                Layout.fillWidth: true
                model: clinico.mascotas
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige un paciente")
            }

            LabeledField { id: consultaMotivo; Layout.fillWidth: true; label: qsTr("Motivo de la visita"); placeholder: qsTr("Control anual, cojera…") }
            CheckBox { id: consultaEmergencia; text: qsTr("Es una emergencia") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoConsulta.close() },
            PrimaryButton {
                text: qsTr("Abrir consulta")
                onClicked: {
                    clinico.abrirConsulta(comboMascotaConsulta.currentIndex >= 0
                                          ? clinico.mascotas[comboMascotaConsulta.currentIndex].uuid
                                          : "",
                                          root.citaActual,
                                          consultaMotivo.text,
                                          consultaEmergencia.checked)
                    if (clinico.error.length === 0)
                        dialogoConsulta.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoCerrarConsulta
        titulo: qsTr("Cerrar consulta")
        descripcion: qsTr("El diagnóstico queda firmado y la cita pasa a completada.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField { id: cierreDiagnostico; Layout.fillWidth: true; label: qsTr("Diagnóstico") }
            LabeledField { id: cierreTratamiento; Layout.fillWidth: true; label: qsTr("Tratamiento indicado"); placeholder: qsTr("Opcional") }
            LabeledField { id: cierreExamen; Layout.fillWidth: true; label: qsTr("Examen físico"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCerrarConsulta.close() },
            PrimaryButton {
                text: qsTr("Firmar y cerrar")
                onClicked: {
                    clinico.cerrarConsulta(root.consultaActual, cierreDiagnostico.text,
                                           cierreTratamiento.text, cierreExamen.text)
                    if (clinico.error.length === 0)
                        dialogoCerrarConsulta.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoReceta
        titulo: qsTr("Receta")
        descripcion: qsTr("El precio se congela al emitir; el stock se descuenta solo si lo indicas.")
        width: 760

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: comboConsultaReceta
                Layout.fillWidth: true
                model: clinico.consultasAbiertas
                textRole: "mascota"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige una consulta abierta")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ComboBox {
                    id: recetaProducto
                    Layout.preferredWidth: 220
                    model: clinico.productos
                    textRole: "nombre"
                    displayText: currentIndex >= 0 ? currentText : qsTr("Sin producto")
                }
                LabeledField { id: recetaMedicamento; Layout.fillWidth: true; label: qsTr("Medicamento") }
                LabeledField { id: recetaDosis; Layout.preferredWidth: 120; label: qsTr("Dosis") }
                LabeledField { id: recetaFrecuencia; Layout.preferredWidth: 130; label: qsTr("Frecuencia") }
                LabeledField { id: recetaDuracion; Layout.preferredWidth: 120; label: qsTr("Duración") }
                LabeledField { id: recetaCantidad; Layout.preferredWidth: 90; label: qsTr("Cant."); text: "1"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: recetaPrecio; Layout.preferredWidth: 110; label: qsTr("Precio"); text: "0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                SecondaryButton {
                    text: qsTr("Añadir")
                    onClicked: {
                        if (recetaMedicamento.text.length === 0)
                            return
                        modeloReceta.append({
                            productoUuid: recetaProducto.currentIndex >= 0
                                          ? clinico.productos[recetaProducto.currentIndex].uuid : "",
                            medicamento: recetaMedicamento.text,
                            dosis: recetaDosis.text,
                            frecuencia: recetaFrecuencia.text,
                            duracion: recetaDuracion.text,
                            cantidad: Number(recetaCantidad.text) || 1,
                            precio: Number(recetaPrecio.text) || 0
                        })
                        recetaMedicamento.text = ""
                        recetaDosis.text = ""
                        recetaFrecuencia.text = ""
                        recetaDuracion.text = ""
                        recetaCantidad.text = "1"
                        recetaPrecio.text = "0"
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                clip: true
                spacing: 4
                model: modeloReceta

                delegate: RowLayout {
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 34
                    spacing: 8
                    Label { Layout.fillWidth: true; text: modelData.medicamento + " · " + modelData.dosis + " · " + modelData.frecuencia; font.pixelSize: 11; color: estilo.ink; elide: Text.ElideRight }
                    Label { text: modelData.cantidad + " × $" + Number(modelData.precio).toFixed(2); font.pixelSize: 11; color: estilo.textSecondary }
                    SecondaryButton { text: "×"; onClicked: modeloReceta.remove(index) }
                }
            }

            LabeledField { id: recetaIndicaciones; Layout.fillWidth: true; label: qsTr("Indicaciones generales"); placeholder: qsTr("Opcional") }
            CheckBox { id: recetaStock; text: qsTr("Descontar stock del producto seleccionado") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoReceta.close() },
            PrimaryButton {
                text: qsTr("Emitir receta")
                onClicked: {
                    var lineas = []
                    for (var i = 0; i < modeloReceta.count; ++i) {
                        var linea = modeloReceta.get(i)
                        lineas.push({
                            productoUuid: linea.productoUuid,
                            medicamento: linea.medicamento,
                            dosis: linea.dosis,
                            frecuencia: linea.frecuencia,
                            duracion: linea.duracion,
                            cantidad: linea.cantidad,
                            precio: linea.precio
                        })
                    }
                    clinico.emitirReceta(root.consultaActual, recetaIndicaciones.text,
                                         lineas, recetaStock.checked)
                    if (clinico.error.length === 0)
                        dialogoReceta.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoCargo
        titulo: qsTr("Añadir cargo")
        descripcion: qsTr("El cargo queda pendiente hasta que caja lo factura.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: cargoOrigenTipo
                Layout.fillWidth: true
                model: ["Consulta", "Receta", "Laboratorio", "Cirugia", "Hospitalizacion",
                        "Producto", "Servicio", "Otro"]
            }
            LabeledField { id: cargoOrigenUuid; Layout.fillWidth: true; label: qsTr("UUID del origen"); placeholder: qsTr("Opcional") }
            LabeledField { id: cargoConcepto; Layout.fillWidth: true; label: qsTr("Concepto") }
            LabeledField { id: cargoMonto; Layout.fillWidth: true; label: qsTr("Monto"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCargo.close() },
            PrimaryButton {
                text: qsTr("Añadir")
                onClicked: {
                    clinico.agregarCargo(cargoOrigenTipo.currentText, cargoOrigenUuid.text,
                                         cargoConcepto.text, Number(cargoMonto.text))
                    if (clinico.error.length === 0)
                        dialogoCargo.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoLaboratorio
        titulo: qsTr("Solicitar laboratorio")
        descripcion: qsTr("El resultado se proyecta al portal del dueño al marcarlo como listo.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: comboMascotaLab
                Layout.fillWidth: true
                model: clinico.mascotas
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige un paciente")
            }
            ComboBox {
                id: comboConsultaLab
                Layout.fillWidth: true
                model: clinico.consultasAbiertas
                textRole: "mascota"
                displayText: currentIndex >= 0 ? currentText : qsTr("Sin consulta asociada")
            }
            ComboBox {
                id: comboExamenLab
                Layout.fillWidth: true
                model: clinico.examenes
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige un examen")
            }
            ComboBox { id: labPrioridad; Layout.fillWidth: true; model: ["Normal", "Urgente"] }
            LabeledField { id: labNotas; Layout.fillWidth: true; label: qsTr("Observaciones"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoLaboratorio.close() },
            PrimaryButton {
                text: qsTr("Solicitar")
                onClicked: {
                    clinico.solicitarLaboratorio(comboMascotaLab.currentIndex >= 0
                                                 ? clinico.mascotas[comboMascotaLab.currentIndex].uuid
                                                 : "",
                                                 comboConsultaLab.currentIndex >= 0
                                                 ? clinico.consultasAbiertas[comboConsultaLab.currentIndex].uuid
                                                 : "",
                                                 comboExamenLab.currentIndex >= 0
                                                 ? clinico.examenes[comboExamenLab.currentIndex].uuid
                                                 : "",
                                                 labPrioridad.currentText, labNotas.text)
                    if (clinico.error.length === 0)
                        dialogoLaboratorio.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoResultado
        titulo: qsTr("Resultado de laboratorio")
        descripcion: qsTr("Al guardarlo, la solicitud pasa a Listo y el dueño puede verla.")

        LabeledField { id: resultadoTexto; Layout.fillWidth: true; label: qsTr("Resultado"); placeholder: qsTr("Valores, interpretación…") }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoResultado.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    clinico.resultadoLaboratorio(root.laboratorioActual, resultadoTexto.text)
                    if (clinico.error.length === 0)
                        dialogoResultado.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoEvolucion
        titulo: qsTr("Evolución")
        descripcion: qsTr("Nota de seguimiento del paciente hospitalizado.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField { id: evolucionNota; Layout.fillWidth: true; label: qsTr("Nota") }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                LabeledField { id: evolucionPeso; Layout.fillWidth: true; label: qsTr("Peso (kg)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: evolucionTemperatura; Layout.fillWidth: true; label: qsTr("Temperatura (°C)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: evolucionCardiaca; Layout.fillWidth: true; label: qsTr("FC (lpm)"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoEvolucion.close() },
            PrimaryButton {
                text: qsTr("Registrar")
                onClicked: {
                    clinico.registrarEvolucion(root.hospitalizacionActual, evolucionNota.text,
                                               Number(evolucionPeso.text),
                                               Number(evolucionTemperatura.text),
                                               Number(evolucionCardiaca.text))
                    if (clinico.error.length === 0)
                        dialogoEvolucion.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoAlta
        titulo: qsTr("Alta de hospitalización")
        descripcion: qsTr("El estado final queda en el expediente del paciente.")

        ComboBox {
            id: altaEstado
            Layout.fillWidth: true
            model: ["Alta", "Traslado", "Fallecido"]
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoAlta.close() },
            PrimaryButton {
                text: qsTr("Registrar alta")
                onClicked: {
                    clinico.darAlta(root.hospitalizacionActual, altaEstado.currentText)
                    if (clinico.error.length === 0)
                        dialogoAlta.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoCirugia
        titulo: qsTr("Programar cirugía")
        descripcion: qsTr("El procedimiento aparece en el quirófano y en la agenda del día.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            ComboBox {
                id: comboMascotaCirugia
                Layout.fillWidth: true
                model: clinico.mascotas
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige un paciente")
            }
            LabeledField { id: cirugiaProcedimiento; Layout.fillWidth: true; label: qsTr("Procedimiento") }
            LabeledField { id: cirugiaFecha; Layout.fillWidth: true; label: qsTr("Fecha y hora"); placeholder: qsTr("2026-03-12 09:00") }
            LabeledField { id: cirugiaAnestesia; Layout.fillWidth: true; label: qsTr("Anestesia"); placeholder: qsTr("General inhalada") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCirugia.close() },
            PrimaryButton {
                text: qsTr("Programar")
                onClicked: {
                    clinico.programarCirugia(comboMascotaCirugia.currentIndex >= 0
                                             ? clinico.mascotas[comboMascotaCirugia.currentIndex].uuid
                                             : "",
                                             "", "", cirugiaProcedimiento.text,
                                             cirugiaFecha.text, cirugiaAnestesia.text)
                    if (clinico.error.length === 0)
                        dialogoCirugia.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoCerrarCirugia
        titulo: qsTr("Cerrar cirugía")
        descripcion: qsTr("Registra el diagnóstico postoperatorio y las notas de quirófano.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField { id: cirugiaDiagnostico; Layout.fillWidth: true; label: qsTr("Diagnóstico postoperatorio") }
            LabeledField { id: cirugiaNotas; Layout.fillWidth: true; label: qsTr("Notas"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCerrarCirugia.close() },
            PrimaryButton {
                text: qsTr("Completar")
                onClicked: {
                    clinico.cerrarCirugia(root.cirugiaActual, cirugiaDiagnostico.text,
                                          cirugiaNotas.text)
                    if (clinico.error.length === 0)
                        dialogoCerrarCirugia.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoEstetica
        titulo: qsTr("Registrar estética")
        descripcion: qsTr("Si marcas cobrar, el cargo queda pendiente en caja.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            ComboBox {
                id: comboMascotaEstetica
                Layout.fillWidth: true
                model: clinico.mascotas
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Elige un paciente")
            }
            ComboBox {
                id: comboEsteticaTipo
                Layout.fillWidth: true
                model: ["Baño", "Corte", "Baño y corte", "Deslanado", "Otro"]
            }
            LabeledField { id: esteticaPrecio; Layout.fillWidth: true; label: qsTr("Precio"); inputMethodHints: Qt.ImhFormattedNumbersOnly }
            LabeledField { id: esteticaObservaciones; Layout.fillWidth: true; label: qsTr("Observaciones"); placeholder: qsTr("Opcional") }
            CheckBox { id: esteticaCobrar; text: qsTr("Crear cargo pendiente") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoEstetica.close() },
            PrimaryButton {
                text: qsTr("Registrar")
                onClicked: {
                    clinico.registrarEstetica(comboMascotaEstetica.currentIndex >= 0
                                              ? clinico.mascotas[comboMascotaEstetica.currentIndex].uuid
                                              : "",
                                              "", "", comboEsteticaTipo.currentText,
                                              Number(esteticaPrecio.text),
                                              esteticaObservaciones.text,
                                              esteticaCobrar.checked)
                    if (clinico.error.length === 0)
                        dialogoEstetica.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoFacturar
        titulo: qsTr("Facturar cargo")
        descripcion: qsTr("El cargo pasa a pagado cuando registras el método de pago.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            Label { Layout.fillWidth: true; text: root.conceptoActual; font.pixelSize: 14; font.bold: true; color: estilo.ink }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                LabeledField { id: facturaMonto; Layout.fillWidth: true; label: qsTr("Monto"); enabled: false }
                LabeledField { id: facturaDescuento; Layout.fillWidth: true; label: qsTr("Descuento"); text: "0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: facturaImpuesto; Layout.fillWidth: true; label: qsTr("Impuesto"); text: "0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }
            ComboBox {
                id: facturaMetodo
                Layout.fillWidth: true
                model: ["Efectivo", "Tarjeta", "Transferencia", "Credito"]
            }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoFacturar.close() },
            PrimaryButton {
                text: qsTr("Emitir factura")
                onClicked: {
                    clinico.facturarCargos("", [root.cargoActual], facturaMetodo.currentText,
                                           Number(facturaDescuento.text),
                                           Number(facturaImpuesto.text))
                    if (clinico.error.length === 0)
                        dialogoFacturar.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoCaja
        titulo: qsTr("Cierre de caja")
        descripcion: qsTr("Cuenta el dinero físico antes de cerrar. El desfase queda registrado.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField { id: cajaReal; Layout.fillWidth: true; label: qsTr("Ingresos reales"); text: "0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            LabeledField { id: cajaObservaciones; Layout.fillWidth: true; label: qsTr("Observaciones"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCaja.close() },
            PrimaryButton {
                text: qsTr("Cerrar caja")
                onClicked: {
                    clinico.cerrarCaja(Number(cajaReal.text), cajaObservaciones.text)
                    if (clinico.error.length === 0)
                        dialogoCaja.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoCatalogo
        titulo: root.modoCatalogo === "servicio" ? qsTr("Nuevo servicio") : qsTr("Nuevo examen")
        descripcion: qsTr("El catálogo se sincroniza con el resto del nodo.")

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            LabeledField { id: catalogoCodigo; Layout.fillWidth: true; label: qsTr("Código") }
            LabeledField { id: catalogoNombre; Layout.fillWidth: true; label: qsTr("Nombre") }
            LabeledField { id: catalogoPrecio; Layout.fillWidth: true; label: qsTr("Precio"); visible: root.modoCatalogo === "servicio"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            CheckBox { id: catalogoActivo; text: qsTr("Activo"); checked: true }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCatalogo.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    if (root.modoCatalogo === "servicio")
                        clinico.guardarServicio("", catalogoCodigo.text, catalogoNombre.text,
                                                Number(catalogoPrecio.text), catalogoActivo.checked)
                    else
                        clinico.guardarExamen("", catalogoCodigo.text, catalogoNombre.text,
                                              catalogoActivo.checked)
                    if (clinico.error.length === 0)
                        dialogoCatalogo.close()
                }
            }
        ]
    }

    ListModel { id: modeloReceta }
}
