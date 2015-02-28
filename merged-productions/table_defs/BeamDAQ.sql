-- MySQL dump 10.13  Distrib 5.5.41, for debian-linux-gnu (x86_64)
--
-- Host: e906-db3.fnal.gov    Database: merged_roadset57_R004
-- ------------------------------------------------------
-- Server version	5.1.73

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `BeamDAQ`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE IF NOT EXISTS `BeamDAQ` (
  `spillID` int(10) unsigned NOT NULL,
  `timestamp` datetime NOT NULL,
  `NM3ION` float NOT NULL,
  `QIEsum` float NOT NULL,
  `dutyfactor53MHz` decimal(8,4) NOT NULL,
  `inhibit_count` int(11) NOT NULL,
  `inhibit_block_sum` float NOT NULL,
  `trigger_count` int(11) NOT NULL,
  `trigger_sum_no_inhibit` float NOT NULL,
  `Inh_output_delay` int(11) NOT NULL,
  `QIE_inh_delay` int(11) NOT NULL,
  `Min_Inh_Width` int(11) NOT NULL,
  `Inh_thres` int(11) NOT NULL,
  `QIE_busy_delay` int(11) NOT NULL,
  `Marker_delay` int(11) NOT NULL,
  `QIE_phase_adjust` int(11) NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
/*!40101 SET character_set_client = @saved_cs_client */;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-02-24 17:45:37
