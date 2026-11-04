 #include <iostream>

 #include "src/cpu/ArmCpu.hpp"
 #include "src/memory/Bus.hpp"


int main() {
    std::cout << "Hello World!";
    return 0;
}


//#include <gtest/gtest.h>
//
//// Una función sencilla que queremos probar
//int sumar(int a, int b) {
//    return a + b;
//}
//
//// Declaración de un caso de prueba usando la macro TEST
//TEST(SumaTest, NumerosPositivos) {
//    EXPECT_EQ(sumar(2, 3), 5);
//}
//
//// Función main predeterminada que ejecuta todas las pruebas
//int main(int argc, char **argv) {
//    ::testing::InitGoogleTest(&argc, argv);
//    return RUN_ALL_TESTS();
//}