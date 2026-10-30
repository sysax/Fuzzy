import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Gestión de inventario: stock crítico, caducidades y precios.
Item {
    id: root

    property bool enDetalle: (inventario.seleccionado.uuid || "").length > 0

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
            titulo: qsTr("Inventario")
            subtitulo: qsTr("%1 productos · %2 bajo mínimo · %3 por vencer").arg(inventario.total).arg(inventario.criticos).arg(inventario.porVencer)

            PrimaryButton {
                text: qsTr("Nuevo producto")
                onClicked: {
                    inventario.limpiarMensajes()
                    dialogoProducto.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            KpiCard { titulo: qsTr("Valor del inventario"); valor: inventario.valorInventarioTexto; subtexto: qsTr("a precio de compra"); icono: "box-seam" }
            KpiCard { titulo: qsTr("Stock crítico"); valor: inventario.criticos; subtexto: qsTr("en o bajo el mínimo"); icono: "exclamation-triangle" }
            KpiCard { titulo: qsTr("Por vencer"); valor: inventario.porVencer; subtexto: qsTr("próximos 60 días"); icono: "calendar-check" }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SearchBox {
                Layout.preferredWidth: 320
                placeholder: qsTr("Buscar producto o código…")
                onEditado: inventario.buscar(texto)
            }

            RowLayout {
                spacing: 4
                Repeater {
                    model: [
                        { clave: "todos", texto: qsTr("Todos") },
                        { clave: "critico", texto: qsTr("Stock crítico") },
                        { clave: "vencer", texto: qsTr("Por vencer") }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        width: 118
                        height: 36
                        radius: estilo.radiusPill
                        color: inventario.vista === modelData.clave ? estilo.ink : estilo.surface
                        border.width: 1
                        border.color: inventario.vista === modelData.clave ? estilo.ink : estilo.border

                        Label {
                            anchors.centerIn: parent
                            text: modelData.texto
                            font.pixelSize: 12
                            color: inventario.vista === modelData.clave ? "#FFFFFF" : estilo.textSecondary
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: inventario.cambiarVista(modelData.clave)
                        }
                    }
                }
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
                Layout.preferredWidth: 640
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
                            Label { Layout.fillWidth: true; text: qsTr("PRODUCTO"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 110; text: qsTr("CATEGORÍA"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 90; text: qsTr("STOCK"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 100; text: qsTr("VENTA"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                            Label { Layout.preferredWidth: 110; text: qsTr("CADUCIDAD"); font.pixelSize: 10; font.letterSpacing: 1; color: estilo.textMuted }
                        }
                    }

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: inventario.productos.length === 0
                        icono: "box-seam"
                        titulo: qsTr("Sin productos")
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: inventario.productos.length > 0
                        clip: true
                        model: inventario.productos

                        delegate: Item {
                            required property var modelData
                            width: ListView.view.width
                            height: 54

                            Rectangle {
                                anchors.fill: parent
                                color: inventario.seleccionado.uuid === modelData.uuid ? estilo.background : "transparent"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 9
                                        color: modelData.critico ? estilo.warningBg : estilo.chip
                                        AppIcon {
                                            anchors.centerIn: parent
                                            nombre: "capsule"
                                            color: modelData.critico ? estilo.warning : estilo.textSecondary
                                            tamano: 14
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Label {
                                            text: modelData.nombre
                                            font.pixelSize: 12
                                            font.bold: true
                                            color: estilo.ink
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: modelData.principio.length > 0 ? modelData.principio : modelData.codigo
                                            font.pixelSize: 10
                                            color: estilo.textMuted
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }

                                Label { Layout.preferredWidth: 110; text: modelData.categoria; font.pixelSize: 11; color: estilo.textSecondary; elide: Text.ElideRight }
                                Label {
                                    Layout.preferredWidth: 90
                                    text: modelData.stock + " / " + modelData.minimo
                                    font.pixelSize: 11
                                    font.bold: modelData.critico
                                    color: modelData.critico ? estilo.warning : estilo.ink
                                }
                                Label { Layout.preferredWidth: 100; text: modelData.ventaTexto; font.pixelSize: 11; color: estilo.ink }
                                Label { Layout.preferredWidth: 110; text: modelData.caducidad.length > 0 ? modelData.caducidad : "—"; font.pixelSize: 11; color: estilo.textSecondary }
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
                                onClicked: inventario.seleccionar(modelData.uuid)
                            }
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 330
                titulo: root.enDetalle ? (inventario.seleccionado.nombre || "") : qsTr("Detalle del producto")
                subtitulo: root.enDetalle ? qsTr("Proveedor: %1").arg(inventario.seleccionado.proveedor || qsTr("sin proveedor"))
                                          : qsTr("Selecciona un producto")

                EmptyState {
                    anchors.fill: parent
                    visible: !root.enDetalle
                    icono: "box-seam"
                    titulo: qsTr("Ningún producto seleccionado")
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    visible: root.enDetalle

                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Stock actual"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: inventario.seleccionado.stock || 0; font.pixelSize: 20; font.bold: true; color: inventario.seleccionado.stock <= inventario.seleccionado.minimo ? estilo.warning : estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Mínimo"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: inventario.seleccionado.minimo || 0; font.pixelSize: 12; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Compra"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: "$" + (inventario.seleccionado.compra || 0).toFixed(2); font.pixelSize: 12; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Venta"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: "$" + (inventario.seleccionado.venta || 0).toFixed(2); font.pixelSize: 12; color: estilo.ink }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: qsTr("Caducidad"); font.pixelSize: 12; color: estilo.textMuted }
                        Label { text: inventario.seleccionado.caducidad || "—"; font.pixelSize: 12; color: estilo.ink }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: estilo.border }

                    Label { text: qsTr("Ajustar stock"); font.pixelSize: 11; color: estilo.textMuted }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        SecondaryButton { text: "−10"; onClicked: inventario.ajustarStock(inventario.seleccionado.uuid, -10, qsTr("Ajuste manual")) }
                        SecondaryButton { text: "−1"; onClicked: inventario.ajustarStock(inventario.seleccionado.uuid, -1, qsTr("Ajuste manual")) }
                        SecondaryButton { text: "+1"; onClicked: inventario.ajustarStock(inventario.seleccionado.uuid, 1, qsTr("Recepción")) }
                        SecondaryButton { text: "+10"; onClicked: inventario.ajustarStock(inventario.seleccionado.uuid, 10, qsTr("Recepción")) }
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: inventario.seleccionado.pedido ? qsTr("Marcar pedido recibido") : qsTr("Marcar pedido en curso")
                        onClicked: inventario.marcarPedido(inventario.seleccionado.uuid, !inventario.seleccionado.pedido)
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: qsTr("Editar producto")
                        onClicked: {
                            inventario.limpiarMensajes()
                            dialogoEditar.open()
                        }
                    }

                    StatusChip { estado: inventario.seleccionado.pedido ? qsTr("Pedido en curso") : qsTr("Sin pedido") }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    AppDialog {
        id: dialogoProducto
        titulo: qsTr("Nuevo producto")
        descripcion: qsTr("El stock inicial queda sujeto a ajustes posteriores.")

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: productoNombre; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Nombre comercial"); placeholder: qsTr("Amoxicilina 250 mg") }
            LabeledField { id: productoPrincipio; Layout.fillWidth: true; label: qsTr("Principio activo"); placeholder: qsTr("Amoxicilina") }
            ComboBox {
                id: comboCategoria
                Layout.fillWidth: true
                model: inventario.categorias
                textRole: "nombre"
                displayText: currentIndex >= 0 ? currentText : qsTr("Categoría")
            }
            LabeledField { id: productoCodigo; Layout.fillWidth: true; label: qsTr("Código"); placeholder: qsTr("Opcional") }
            LabeledField { id: productoUnidad; Layout.fillWidth: true; label: qsTr("Unidad"); text: qsTr("unidades") }
            LabeledField { id: productoStock; Layout.fillWidth: true; label: qsTr("Stock inicial"); text: "0"; inputMethodHints: Qt.ImhDigitsOnly }
            LabeledField { id: productoMinimo; Layout.fillWidth: true; label: qsTr("Stock mínimo"); text: "5"; inputMethodHints: Qt.ImhDigitsOnly }
            LabeledField { id: productoCompra; Layout.fillWidth: true; label: qsTr("Precio compra"); text: "0.00" }
            LabeledField { id: productoVenta; Layout.fillWidth: true; label: qsTr("Precio venta"); text: "0.00" }
            LabeledField { id: productoCaducidad; Layout.fillWidth: true; label: qsTr("Caducidad"); placeholder: "yyyy-MM-dd" }
            LabeledField { id: productoProveedor; Layout.fillWidth: true; label: qsTr("Proveedor"); placeholder: qsTr("Opcional") }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoProducto.close() },
            PrimaryButton {
                text: qsTr("Crear")
                onClicked: {
                    inventario.crearProducto(productoNombre.text, productoPrincipio.text,
                                             comboCategoria.currentIndex >= 0 ? inventario.categorias[comboCategoria.currentIndex].uuid : "",
                                             productoCodigo.text, productoUnidad.text,
                                             Number(productoStock.text), Number(productoMinimo.text),
                                             Number(productoCompra.text), Number(productoVenta.text),
                                             productoCaducidad.text, productoProveedor.text)
                    if (inventario.error.length === 0)
                        dialogoProducto.close()
                }
            }
        ]
    }

    AppDialog {
        id: dialogoEditar
        titulo: qsTr("Editar producto")
        descripcion: inventario.seleccionado.nombre || ""

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            LabeledField { id: editarNombre; Layout.fillWidth: true; Layout.columnSpan: 2; label: qsTr("Nombre comercial"); text: inventario.seleccionado.nombre || "" }
            LabeledField { id: editarPrincipio; Layout.fillWidth: true; label: qsTr("Principio activo"); text: inventario.seleccionado.principio || "" }
            LabeledField { id: editarCodigo; Layout.fillWidth: true; label: qsTr("Código"); text: inventario.seleccionado.codigo || "" }
            LabeledField { id: editarStock; Layout.fillWidth: true; label: qsTr("Stock"); text: inventario.seleccionado.stock || 0; inputMethodHints: Qt.ImhDigitsOnly }
            LabeledField { id: editarMinimo; Layout.fillWidth: true; label: qsTr("Mínimo"); text: inventario.seleccionado.minimo || 0; inputMethodHints: Qt.ImhDigitsOnly }
            LabeledField { id: editarCompra; Layout.fillWidth: true; label: qsTr("Compra"); text: (inventario.seleccionado.compra || 0).toFixed(2) }
            LabeledField { id: editarVenta; Layout.fillWidth: true; label: qsTr("Venta"); text: (inventario.seleccionado.venta || 0).toFixed(2) }
            LabeledField { id: editarCaducidad; Layout.fillWidth: true; label: qsTr("Caducidad"); text: inventario.seleccionado.caducidad || "" }
            LabeledField { id: editarProveedor; Layout.fillWidth: true; label: qsTr("Proveedor"); text: inventario.seleccionado.proveedor || "" }
        }

        acciones: [
            SecondaryButton { text: qsTr("Cancelar"); onClicked: dialogoEditar.close() },
            PrimaryButton {
                text: qsTr("Guardar")
                onClicked: {
                    inventario.actualizarProducto(inventario.seleccionado.uuid, editarNombre.text,
                                                  editarPrincipio.text, inventario.seleccionado.categoriaUuid,
                                                  editarCodigo.text, inventario.seleccionado.unidad,
                                                  Number(editarStock.text), Number(editarMinimo.text),
                                                  Number(editarCompra.text), Number(editarVenta.text),
                                                  editarCaducidad.text, editarProveedor.text)
                    if (inventario.error.length === 0)
                        dialogoEditar.close()
                }
            }
        ]
    }
}
