import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Facturación: listado, detalle, emisión y cierre de caja.
Item {
    id: root

    property bool enDetalle: (facturacion.seleccionada.uuid || "").length > 0

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
            titulo: qsTr("Facturación")
            subtitulo: qsTr("%1 facturas · %2 pendientes de cobro").arg(facturacion.total).arg(facturacion.pendientes)

            SecondaryButton {
                text: qsTr("Cerrar caja")
                onClicked: {
                    facturacion.limpiarMensajes()
                    dialogoCaja.open()
                }
            }
            PrimaryButton {
                text: qsTr("Nueva factura")
                onClicked: {
                    facturacion.limpiarMensajes()
                    facturacion.iniciarBorrador("")
                    dialogoFactura.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            KpiCard { titulo: qsTr("Facturado (pagado)"); valor: facturacion.totalFacturadoTexto; subtexto: qsTr("histórico del nodo"); icono: "receipt" }
            KpiCard { titulo: qsTr("Caja de hoy"); valor: facturacion.resumenCaja.totalTexto || "$0"; subtexto: qsTr("%1 pagos").arg(facturacion.resumenCaja.cantidad || 0); icono: "cash-coin" }
            KpiCard { titulo: qsTr("Efectivo"); valor: facturacion.resumenCaja.efectivoTexto || "$0"; subtexto: qsTr("cierre del día"); icono: "cash" }
            KpiCard { titulo: qsTr("Pendiente de cobro"); valor: facturacion.resumenCaja.pendienteTexto || "$0"; subtexto: qsTr("facturas emitidas"); icono: "hourglass-split" }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SearchBox {
                Layout.preferredWidth: 310
                placeholder: qsTr("Buscar factura o cliente…")
                onEditado: facturacion.buscar(texto)
            }

            ComboBox {
                Layout.preferredWidth: 190
                model: [qsTr("Todos los estados"), qsTr("Pagada"), qsTr("Emitida"), qsTr("Pendiente")]
                onActivated: facturacion.filtrarEstado(currentIndex === 0 ? "" : currentText)
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 620
                margen: 0

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        color: estilo.background

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12
                            Label { Layout.preferredWidth: 130; text: qsTr("FACTURA"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.fillWidth: true; text: qsTr("CLIENTE"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 110; text: qsTr("TOTAL"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 90; text: qsTr("ESTADO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        }
                    }

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: facturacion.facturas.length === 0
                        icono: "receipt"
                        titulo: qsTr("Sin facturas")
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: facturacion.facturas.length > 0
                        clip: true
                        model: facturacion.facturas

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 56

                            Rectangle {
                                anchors.fill: parent
                                color: facturacion.seleccionada.uuid === modelData.uuid ? estilo.background : "transparent"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                spacing: 12

                                ColumnLayout {
                                    Layout.preferredWidth: 130
                                    spacing: 0
                                    Label { text: modelData.numero; font.pixelSize: 12; font.bold: true; color: estilo.ink }
                                    Label { text: modelData.fecha; font.pixelSize: 10; color: estilo.textMuted }
                                }
                                Label { Layout.fillWidth: true; text: modelData.cliente; font.pixelSize: 12; color: estilo.ink; elide: Text.ElideRight }
                                Label { Layout.preferredWidth: 110; text: modelData.totalTexto; font.pixelSize: 12; font.bold: true; color: estilo.ink }
                                StatusChip { Layout.preferredWidth: 90; estado: modelData.estado }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: estilo.border
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: facturacion.seleccionar(modelData.uuid)
                            }
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 350
                titulo: root.enDetalle ? qsTr("Factura %1").arg(facturacion.seleccionada.numero || "")
                                       : qsTr("Detalle de factura")
                subtitulo: root.enDetalle ? (facturacion.seleccionada.cliente || "") : qsTr("Selecciona una factura")

                EmptyState {
                    anchors.fill: parent
                    visible: !root.enDetalle
                    icono: "receipt"
                    titulo: qsTr("Ninguna factura seleccionada")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    visible: root.enDetalle

                    RowLayout {
                        Layout.fillWidth: true
                        StatusChip { estado: facturacion.seleccionada.estado || "" }
                        Item { Layout.fillWidth: true }
                        Label { text: facturacion.seleccionada.fecha || ""; font.pixelSize: 10; color: estilo.textMuted }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: facturacion.detalles

                        delegate: RowLayout {
                            required property var modelData
                            width: ListView.view.width
                            height: 38
                            spacing: 8
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label { text: modelData.nombre; font.pixelSize: 11; color: estilo.ink; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label { text: modelData.cantidad + " × $" + modelData.precio.toFixed(2); font.pixelSize: 9; color: estilo.textMuted }
                            }
                            Label { text: "$" + modelData.total.toFixed(2); font.pixelSize: 11; font.bold: true; color: estilo.ink }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: estilo.border }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Subtotal"); font.pixelSize: 11; color: estilo.textMuted }
                        Label { text: "$" + (facturacion.seleccionada.subtotal || 0).toFixed(2); font.pixelSize: 11; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Impuesto"); font.pixelSize: 11; color: estilo.textMuted }
                        Label { text: "$" + (facturacion.seleccionada.impuesto || 0).toFixed(2); font.pixelSize: 11; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Descuento"); font.pixelSize: 11; color: estilo.textMuted }
                        Label { text: "−$" + (facturacion.seleccionada.descuento || 0).toFixed(2); font.pixelSize: 11; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Total"); font.pixelSize: 13; font.bold: true; color: estilo.ink }
                        Label { text: facturacion.seleccionada.totalTexto || "$0"; font.pixelSize: 17; font.bold: true; color: estilo.ink }
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        visible: facturacion.seleccionada.estado !== "Pagada"
                        text: qsTr("Marcar como pagada")
                        onClicked: facturacion.marcarPagada(facturacion.seleccionada.uuid)
                    }
                    SecondaryButton {
                        Layout.fillWidth: true
                        visible: facturacion.seleccionada.estado !== "Pagada"
                        text: qsTr("Devolver a pendiente")
                        onClicked: facturacion.anular(facturacion.seleccionada.uuid)
                    }
                }
            }
        }
    }

    // ── Nueva factura ─────────────────────────────────────────
    AppDialog {
        id: dialogoFactura
        titulo: qsTr("Nueva factura")
        descripcion: qsTr("Agrega servicios y productos; el stock se descuenta al emitir.")
        width: 720

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: comboCliente
                Layout.fillWidth: true
                model: facturacion.clientes
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Consumidor final")
                onActivated: facturacion.iniciarBorrador(facturacion.clientes[currentIndex].uuid)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ComboBox {
                    id: comboConcepto
                    Layout.fillWidth: true
                    model: facturacion.servicios.concat(facturacion.productos)
                    textRole: "nombre"
                    displayText: currentIndex >= 0 ? currentText : qsTr("Elige un servicio o producto")
                }

                SecondaryButton {
                    text: qsTr("Añadir")
                    onClicked: {
                        if (comboConcepto.currentIndex < 0)
                            return
                        if (comboConcepto.currentIndex < facturacion.servicios.length) {
                            facturacion.agregarServicio(facturacion.servicios[comboConcepto.currentIndex].uuid, 1)
                        } else {
                            var indice = comboConcepto.currentIndex - facturacion.servicios.length
                            facturacion.agregarProducto(facturacion.productos[indice].uuid, 1)
                        }
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                clip: true
                spacing: 4
                model: facturacion.lineas

                delegate: RowLayout {
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    height: 38
                    spacing: 8
                    Label { Layout.fillWidth: true; text: modelData.nombre; font.pixelSize: 11; color: estilo.ink; elide: Text.ElideRight }
                    Label { text: modelData.cantidad; font.pixelSize: 11; color: estilo.textSecondary }
                    Label { Layout.preferredWidth: 90; text: modelData.totalTexto; font.pixelSize: 11; font.bold: true; color: estilo.ink; horizontalAlignment: Text.AlignRight }
                    SecondaryButton { text: "×"; onClicked: facturacion.quitarLinea(index) }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                LabeledField { id: facturaDescuento; Layout.fillWidth: true; label: qsTr("Descuento"); text: "0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                LabeledField { id: facturaImpuesto; Layout.fillWidth: true; label: qsTr("Impuesto"); text: "0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                ComboBox {
                    id: comboMetodo
                    Layout.fillWidth: true
                    model: [qsTr("Sin pago"), qsTr("Efectivo"), qsTr("Tarjeta"), qsTr("Transferencia"), qsTr("Credito")]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: qsTr("Total"); font.pixelSize: 14; font.bold: true; color: estilo.ink }
                Label { text: facturacion.totalBorradorTexto; font.pixelSize: 22; font.bold: true; color: estilo.ink }
            }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: { facturacion.cancelarBorrador(); dialogoFactura.close() } },
            PrimaryButton {
                text: qsTr("Emitir")
                onClicked: {
                    facturacion.emitir(comboMetodo.currentIndex <= 0 ? "" : comboMetodo.currentText,
                                       Number(facturaDescuento.text), Number(facturaImpuesto.text))
                    if (facturacion.error.length === 0)
                        dialogoFactura.close()
                }
            }
        ]
    }

    // ── Cierre de caja ────────────────────────────────────────
    AppDialog {
        id: dialogoCaja
        titulo: qsTr("Cierre de caja")
        descripcion: qsTr("Cuenta el dinero físico antes de cerrar. El desfase queda registrado.")

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 10

            Label { text: qsTr("Calculado"); font.pixelSize: 12; color: estilo.textMuted }
            Label { text: facturacion.resumenCaja.totalTexto || "$0"; font.pixelSize: 13; font.bold: true; color: estilo.ink }
            Label { text: qsTr("Efectivo"); font.pixelSize: 12; color: estilo.textMuted }
            Label { text: facturacion.resumenCaja.efectivoTexto || "$0"; font.pixelSize: 12; color: estilo.ink }
            Label { text: qsTr("Tarjeta"); font.pixelSize: 12; color: estilo.textMuted }
            Label { text: facturacion.resumenCaja.tarjetaTexto || "$0"; font.pixelSize: 12; color: estilo.ink }
            Label { text: qsTr("Transferencia"); font.pixelSize: 12; color: estilo.textMuted }
            Label { text: facturacion.resumenCaja.transferenciaTexto || "$0"; font.pixelSize: 12; color: estilo.ink }

            LabeledField { id: cajaReal; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Ingresos reales"); text: "$0"; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            LabeledField { id: cajaObservaciones; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Observaciones"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoCaja.close() },
            PrimaryButton {
                text: qsTr("Cerrar caja")
                onClicked: {
                    facturacion.cerrarCaja(Number(cajaReal.text), cajaObservaciones.text)
                    if (facturacion.error.length === 0)
                        dialogoCaja.close()
                }
            }
        ]
    }
}
